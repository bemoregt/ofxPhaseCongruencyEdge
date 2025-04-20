#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include <vector>

struct PhaseCongruencyConst {
    double sigma;
    double mult = 2.0;
    double minwavelength = 1.5;
    double epsilon = 0.0002;
    double cutOff = 0.4;
    double g = 10.0;
    double k = 10.0;
    PhaseCongruencyConst();
    PhaseCongruencyConst(const PhaseCongruencyConst& _pcc);
    PhaseCongruencyConst& operator=(const PhaseCongruencyConst& _pcc);
};

class PhaseCongruency;

class ofxPhaseCongruencyEdge
{
public:
    ofxPhaseCongruencyEdge();
    ~ofxPhaseCongruencyEdge();
    
    // Initialize with image size, number of scales and orientations
    void setup(int width, int height, int nscales = 4, int norientations = 6);
    
    // Set custom parameters
    void setParameters(PhaseCongruencyConst parameters);
    
    // Compute phase congruency and extract features from image
    void process(const ofImage& image, ofImage& edgeImage, ofImage& cornerImage);
    void process(const cv::Mat& inputMat, cv::Mat& edgeMat, cv::Mat& cornerMat);
    
    // Utility functions
    void drawEdges(float x, float y, float width, float height);
    void drawCorners(float x, float y, float width, float height);
    void drawResults(float x, float y, float width, float height);
    
    // Access result images
    ofImage& getEdgeImage() { return edgeImage; }
    ofImage& getCornerImage() { return cornerImage; }
    
private:
    PhaseCongruency* pc;
    bool isSetup;
    ofImage edgeImage;
    ofImage cornerImage;
    cv::Mat edgeMat;
    cv::Mat cornerMat;
    cv::Size imgSize;
    int nscale;
    int norient;
};