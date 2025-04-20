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