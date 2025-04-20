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