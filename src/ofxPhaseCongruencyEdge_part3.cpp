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