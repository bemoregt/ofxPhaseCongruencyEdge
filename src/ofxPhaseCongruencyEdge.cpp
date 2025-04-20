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