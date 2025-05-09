#include "ofxPhaseCongruencyEdge.h"
#include "ofxCv.h"

using namespace cv;
using namespace ofxCv;

class PhaseCongruency
{
public:
    PhaseCongruency(cv::Size _img_size, size_t _nscale, size_t _norient);
    ~PhaseCongruency() {}
    void setConst(PhaseCongruencyConst _pcc);
    void calc(cv::InputArray _src, std::vector<cv::Mat> &_pc);
    void feature(std::vector<cv::Mat> &_pc, cv::OutputArray _edges, cv::OutputArray _corners);
    void feature(cv::InputArray _src, cv::OutputArray _edges, cv::OutputArray _corners);

private:
    cv::Size size;
    size_t norient;
    size_t nscale;

    PhaseCongruencyConst pcc;

    std::vector<cv::Mat> filter;
};

// Rearrange the quadrants of Fourier image so that the origin is at
// the image center
static void shiftDFT(InputArray _src, OutputArray _dst)
{
    Mat src = _src.getMat();
    cv::Size size = src.size();

    _dst.create(size, src.type());
    auto dst = _dst.getMat();

    const int cx = size.width / 2;
    const int cy = size.height / 2; // image center

    Mat s1 = src(cv::Rect(0, 0, cx, cy));
    Mat s2 = src(cv::Rect(cx, 0, cx, cy));
    Mat s3 = src(cv::Rect(cx, cy, cx, cy));
    Mat s4 = src(cv::Rect(0, cy, cx, cy));

    Mat d1 = dst(cv::Rect(0, 0, cx, cy));
    Mat d2 = dst(cv::Rect(cx, 0, cx, cy));
    Mat d3 = dst(cv::Rect(cx, cy, cx, cy));
    Mat d4 = dst(cv::Rect(0, cy, cx, cy));

    Mat tmp;
    s3.copyTo(tmp);
    s1.copyTo(d3);
    tmp.copyTo(d1);

    s4.copyTo(tmp);
    s2.copyTo(d4);
    tmp.copyTo(d2);
}

#define MAT_TYPE CV_64FC1
#define MAT_TYPE_CNV CV_64F

// Making a filter
// src & dst arrays of equal size & type
PhaseCongruency::PhaseCongruency(cv::Size _size, size_t _nscale, size_t _norient)
{
    size = _size;
    nscale = _nscale;
    norient = _norient;

    filter.resize(nscale * norient);

    const int dft_M = getOptimalDFTSize(_size.height);
    const int dft_N = getOptimalDFTSize(_size.width);

    Mat radius = Mat::zeros(dft_M, dft_N, MAT_TYPE);
    Mat matAr[2];
    matAr[0] = Mat::zeros(dft_M, dft_N, MAT_TYPE);
    matAr[1] = Mat::zeros(dft_M, dft_N, MAT_TYPE);
    Mat lp = Mat::zeros(dft_M, dft_N, MAT_TYPE);
    Mat angular = Mat::zeros(dft_M, dft_N, MAT_TYPE);
    std::vector<Mat> gabor(nscale);

    //Matrix values contain *normalised* radius
    // values ranging from 0 at the centre to
    // 0.5 at the boundary.
    int r;
    const int dft_M_2 = dft_M / 2;
    const int dft_N_2 = dft_N / 2;
    if (dft_M > dft_N) r = dft_N_2;
    else r = dft_M_2;
    const double dr = 1.0 / static_cast<double>(r);
    for (int row = dft_M_2 - r; row < dft_M_2 + r; row++)
    {
        auto radius_row = radius.ptr<double>(row);
        for (int col = dft_N_2 - r; col < dft_N_2 + r; col++)
        {
            int m = (row - dft_M_2);
            int n = (col - dft_N_2);
            radius_row[col] = sqrt(static_cast<double>(m * m + n * n)) * dr;
        }
    }
    lp = radius * 2.5;
    pow(lp, 20.0, lp);
    lp += Scalar::all(1.0);
    radius.at<double>(dft_M_2, dft_N_2) = 1.0;
    // The following implements the log-gabor transfer function.
    double mt = 1.0f;
    for (int scale = 0; scale < nscale; scale++)
    {
        const double wavelength = pcc.minwavelength * mt;
        gabor[scale] = radius * wavelength;
        log(gabor[scale], gabor[scale]);
        pow(gabor[scale], 2.0, gabor[scale]);
        gabor[scale] *= pcc.sigma;
        exp(gabor[scale], gabor[scale]);
        gabor[scale].at<double>(dft_M_2, dft_N_2) = 0.0;
        divide(gabor[scale], lp, gabor[scale]);
        mt = mt * pcc.mult;
    }
    const double angle_const = static_cast<double>(M_PI) / static_cast<double>(norient);
    for (int ori = 0; ori < norient; ori++)
    {
        double angl = (double)ori * angle_const;
        //Now we calculate the angular component that controls the orientation selectivity of the filter.
        for (int i = 0; i < dft_M; i++)
        {
            auto angular_row = angular.ptr<double>(i);
            for (int j = 0; j < dft_N; j++)
            {
                double m = atan2(-((double)j / (double)dft_N - 0.5), (double)i / (double)dft_M - 0.5);
                double s = sin(m);
                double c = cos(m);
                m = s * cos(angl) - c * sin(angl);
                double n = c * cos(angl) + s * sin(angl);
                s = fabs(atan2(m, n));

                angular_row[j] = (cos(min(s * (double)norient * 0.5, M_PI)) + 1.0) * 0.5;
            }
        }
        for (int scale = 0; scale < nscale; scale++)
        {
            multiply(gabor[scale], angular, matAr[0]); //Product of the two components.
            merge(matAr, 2, filter[nscale * ori + scale]);
        }//scale
    }//orientation
    //Filter ready
}

void PhaseCongruency::setConst(PhaseCongruencyConst _pcc)
{
    pcc = _pcc;
}

//Phase congruency calculation
void PhaseCongruency::calc(InputArray _src, std::vector<cv::Mat> &_pc)
{
    Mat src = _src.getMat();

    CV_Assert(src.size() == size);

    const int width = size.width, height = size.height;

    Mat src64;
    src.convertTo(src64, MAT_TYPE_CNV, 1.0 / 255.0);

    const int dft_M_r = getOptimalDFTSize(src.rows) - src.rows;
    const int dft_N_c = getOptimalDFTSize(src.cols) - src.cols;

    _pc.resize(norient);
    std::vector<Mat> eo(nscale);
    Mat complex[2];
    Mat sumAn;
    Mat sumRe;
    Mat sumIm;
    Mat maxAn;
    Mat xEnergy;
    Mat tmp;
    Mat tmp1;
    Mat tmp2;
    Mat energy = Mat::zeros(size, MAT_TYPE);

    //expand input image to optimal size
    Mat padded;
    copyMakeBorder(src64, padded, 0, dft_M_r, 0, dft_N_c, BORDER_CONSTANT, Scalar::all(0));
    Mat planes[] = { Mat_<double>(padded), Mat::zeros(padded.size(), MAT_TYPE_CNV) };

    Mat dft_A;
    merge(planes, 2, dft_A);         // Add to the expanded another plane with zeros
    dft(dft_A, dft_A);            // this way the result may fit in the source matrix

    shiftDFT(dft_A, dft_A);

    for (unsigned o = 0; o < norient; o++)
    {
        double noise = 0;
        for (unsigned scale = 0; scale < nscale; scale++)
        {
            Mat filtered;
            mulSpectrums(dft_A, filter[nscale * o + scale], filtered, 0); // Convolution
            dft(filtered, filtered, DFT_INVERSE);
            filtered(cv::Rect(0, 0, width, height)).copyTo(eo[scale]);

            split(eo[scale], complex);
            Mat eo_mag;
            magnitude(complex[0], complex[1], eo_mag);

            if (scale == 0)
            {
                //here to do noise threshold calculation
                auto tau = mean(eo_mag);
                tau.val[0] = tau.val[0] / sqrt(log(4.0));
                auto mt = 1.0 * pow(pcc.mult, nscale);
                auto totalTau = tau.val[0] * (1.0 - 1.0 / mt) / (1.0 - 1.0 / pcc.mult);
                auto m = totalTau * sqrt(M_PI / 2.0);
                auto n = totalTau * sqrt((4 - M_PI) / 2.0);
                noise = m + pcc.k * n;

                eo_mag.copyTo(maxAn);
                eo_mag.copyTo(sumAn);
                complex[0].copyTo(sumRe);
                complex[1].copyTo(sumIm);
            }
            else
            {
                add(sumAn, eo_mag, sumAn);
                max(eo_mag, maxAn, maxAn);
                add(sumRe, complex[0], sumRe);
                add(sumIm, complex[1], sumIm);
            }
        } // next scale

        magnitude(sumRe, sumIm, xEnergy);
        xEnergy += pcc.epsilon;
        divide(sumIm, xEnergy, sumIm);
        divide(sumRe, xEnergy, sumRe);
        energy.setTo(0);
        for (int scale = 0; scale < nscale; scale++)
        {
            split(eo[scale], complex);

            multiply(complex[0], sumIm, tmp1);
            multiply(complex[1], sumRe, tmp2);

            absdiff(tmp1, tmp2, tmp);
            subtract(energy, tmp, energy);

            multiply(complex[0], sumRe, complex[0]);
            add(energy, complex[0], energy);
            multiply(complex[1], sumIm, complex[1]);
            add(energy, complex[1], energy);
        } //next scale

        energy -= Scalar::all(noise); // -noise
        max(energy, 0.0, energy);
        maxAn += pcc.epsilon;

        divide(sumAn, maxAn, tmp, -1.0 / static_cast<double>(nscale));

        tmp += pcc.cutOff;
        tmp = tmp * pcc.g;
        exp(tmp, tmp);
        tmp += 1.0; // 1 / weight

        //PC
        multiply(tmp, sumAn, tmp);
        divide(energy, tmp, _pc[o]);
    }//orientation
}

//Build up covariance data for every point
void PhaseCongruency::feature(std::vector<cv::Mat>& _pc, cv::OutputArray _edges, cv::OutputArray _corners)
{
    _edges.create(size, CV_8UC1);
    _corners.create(size, CV_8UC1);
    auto edges = _edges.getMat();
    auto corners = _corners.getMat();

    Mat covx2 = Mat::zeros(size, MAT_TYPE);
    Mat covy2 = Mat::zeros(size, MAT_TYPE);
    Mat covxy = Mat::zeros(size, MAT_TYPE);
    Mat cos_pc, sin_pc, mul_pc;

    const double angle_const = M_PI / static_cast<double>(norient);

    for (unsigned o = 0; o < norient; o++)
    {
        auto angl = static_cast<double>(o) * angle_const;
        cos_pc = _pc[o] * cos(angl);
        sin_pc = _pc[o] * sin(angl);
        multiply(cos_pc, sin_pc, mul_pc);
        add(covxy, mul_pc, covxy);
        pow(cos_pc, 2, cos_pc);
        add(covx2, cos_pc, covx2);
        pow(sin_pc, 2, sin_pc);
        add(covy2, sin_pc, covy2);
    } // next orientation

    //Edges calculations
    covx2 *= 2.0 / static_cast<double>(norient);
    covy2 *= 2.0 / static_cast<double>(norient);
    covxy *= 4.0 / static_cast<double>(norient);
    Mat sub;
    subtract(covx2, covy2, sub);

    Mat denom;
    magnitude(sub, covxy, denom); // denom;
    Mat sum;
    add(covy2, covx2, sum);

    Mat minMoment, maxMoment;
    subtract(sum, denom, minMoment);//m = (covy2 + covx2 - denom) / 2;          % ... and minimum moment
    add(sum, denom, maxMoment); //M = (covy2+covx2 + denom)/2;          % Maximum moment

    maxMoment.convertTo(edges, CV_8U, 255);
    minMoment.convertTo(corners, CV_8U, 255);
}

//Build up covariance data for every point
void PhaseCongruency::feature(InputArray _src, cv::OutputArray _edges, cv::OutputArray _corners)
{
    std::vector<cv::Mat> pc;
    calc(_src, pc);
    feature(pc, _edges, _corners);
}

PhaseCongruencyConst::PhaseCongruencyConst()
{
    sigma = -1.0 / (2.0 * log(0.65) * log(0.65));
}

PhaseCongruencyConst::PhaseCongruencyConst(const PhaseCongruencyConst & _pcc)
{
    sigma = _pcc.sigma;
    mult = _pcc.mult;
    minwavelength = _pcc.minwavelength;
    epsilon = _pcc.epsilon;
    cutOff = _pcc.cutOff;
    g = _pcc.g;
    k = _pcc.k;
}

PhaseCongruencyConst& PhaseCongruencyConst::operator=(const PhaseCongruencyConst & _pcc)
{
    if (this == &_pcc) {
        return *this;
    }
    sigma = _pcc.sigma;
    mult = _pcc.mult;
    minwavelength = _pcc.minwavelength;
    epsilon = _pcc.epsilon;
    cutOff = _pcc.cutOff;
    g = _pcc.g;
    k = _pcc.k;

    return *this;
}

// ofxPhaseCongruencyEdge implementation
ofxPhaseCongruencyEdge::ofxPhaseCongruencyEdge() : isSetup(false), pc(nullptr) {
}

ofxPhaseCongruencyEdge::~ofxPhaseCongruencyEdge() {
    if (pc != nullptr) {
        delete pc;
        pc = nullptr;
    }
}

void ofxPhaseCongruencyEdge::setup(int width, int height, int nscales, int norientations) {
    // Clean up previous instance if any
    if (pc != nullptr) {
        delete pc;
        pc = nullptr;
    }
    
    imgSize = cv::Size(width, height);
    nscale = nscales;
    norient = norientations;
    
    // Create the PhaseCongruency instance
    pc = new PhaseCongruency(imgSize, nscale, norient);
    
    // Allocate output image buffers
    edgeImage.allocate(width, height, OF_IMAGE_GRAYSCALE);
    cornerImage.allocate(width, height, OF_IMAGE_GRAYSCALE);
    
    isSetup = true;
}

void ofxPhaseCongruencyEdge::setParameters(PhaseCongruencyConst parameters) {
    if (!isSetup) {
        ofLogError("ofxPhaseCongruencyEdge") << "Setup must be called before setting parameters";
        return;
    }
    
    pc->setConst(parameters);
}

void ofxPhaseCongruencyEdge::process(const ofImage& image, ofImage& edgeImage, ofImage& cornerImage) {
    // Convert to cv::Mat
    cv::Mat inputMat = toCv(image);
    
    // Convert to grayscale if needed
    if (inputMat.channels() > 1) {
        cv::Mat grayMat;
        cv::cvtColor(inputMat, grayMat, cv::COLOR_RGB2GRAY);
        inputMat = grayMat;
    }
    
    // Resize if necessary
    if (inputMat.size() != imgSize) {
        cv::resize(inputMat, inputMat, imgSize);
    }
    
    // Process
    process(inputMat, edgeMat, cornerMat);
    
    // Convert results back to ofImage
    toOf(edgeMat, edgeImage);
    toOf(cornerMat, cornerImage);
    
    edgeImage.update();
    cornerImage.update();
}

void ofxPhaseCongruencyEdge::process(const cv::Mat& inputMat, cv::Mat& edgeMat, cv::Mat& cornerMat) {
    if (!isSetup) {
        ofLogError("ofxPhaseCongruencyEdge") << "Setup must be called before processing";
        return;
    }
    
    // Make sure input is grayscale
    cv::Mat grayMat;
    if (inputMat.channels() > 1) {
        cv::cvtColor(inputMat, grayMat, cv::COLOR_RGB2GRAY);
    } else {
        grayMat = inputMat;
    }
    
    // Resize if necessary
    if (grayMat.size() != imgSize) {
        cv::resize(grayMat, grayMat, imgSize);
    }
    
    // Call the Phase Congruency feature extraction
    pc->feature(grayMat, edgeMat, cornerMat);
    
    // Save results to internal buffers
    toOf(edgeMat, this->edgeImage);
    toOf(cornerMat, this->cornerImage);
    
    this->edgeImage.update();
    this->cornerImage.update();
}

void ofxPhaseCongruencyEdge::drawEdges(float x, float y, float width, float height) {
    if (!isSetup) {
        ofLogError("ofxPhaseCongruencyEdge") << "Setup must be called before drawing";
        return;
    }
    
    edgeImage.draw(x, y, width, height);
}

void ofxPhaseCongruencyEdge::drawCorners(float x, float y, float width, float height) {
    if (!isSetup) {
        ofLogError("ofxPhaseCongruencyEdge") << "Setup must be called before drawing";
        return;
    }
    
    cornerImage.draw(x, y, width, height);
}

void ofxPhaseCongruencyEdge::drawResults(float x, float y, float width, float height) {
    if (!isSetup) {
        ofLogError("ofxPhaseCongruencyEdge") << "Setup must be called before drawing";
        return;
    }
    
    float w = width / 2.0f;
    float h = height;
    
    // Draw edge image on left
    edgeImage.draw(x, y, w, h);
    
    // Draw corner image on right
    cornerImage.draw(x + w, y, w, h);
}