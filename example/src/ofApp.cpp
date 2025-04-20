#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    // Load image
    inputImage.load("test.jpg");
    
    // Initialize the phase congruency detector with 4 scales and 6 orientations
    pc.setup(inputImage.getWidth(), inputImage.getHeight(), 4, 6);
    
    // Process the image
    pc.process(inputImage, edgeImage, cornerImage);
    
    // Set window title
    ofSetWindowTitle("Phase Congruency Example");
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){
    ofSetColor(255);
    
    // Draw original image, edge image, and corner image side by side
    inputImage.draw(0, 0, 512, 512);
    edgeImage.draw(512, 0, 512, 512);
    cornerImage.draw(1024, 0, 512, 512);
    
    // Draw labels
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("Original Image", 10, 20);
    ofDrawBitmapString("Phase Congruency Edges", 522, 20);
    ofDrawBitmapString("Phase Congruency Corners", 1034, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == 's'){
        // Save the output images
        edgeImage.save("pc_edges.png");
        cornerImage.save("pc_corners.png");
        ofLogNotice("ofApp") << "Saved edge and corner images";
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 
    // Allow drag & drop of images
    if(dragInfo.files.size() > 0){
        string filePath = dragInfo.files[0];
        ofFile file(filePath);
        
        if(file.exists() && (ofToLower(file.getExtension()) == "jpg" || 
                             ofToLower(file.getExtension()) == "png" ||
                             ofToLower(file.getExtension()) == "jpeg")){
            
            // Load the new image
            inputImage.load(filePath);
            
            // Re-initialize the processor if image size changed
            if(inputImage.getWidth() != pc.getEdgeImage().getWidth() || 
               inputImage.getHeight() != pc.getEdgeImage().getHeight()){
                pc.setup(inputImage.getWidth(), inputImage.getHeight(), 4, 6);
            }
            
            // Process the new image
            pc.process(inputImage, edgeImage, cornerImage);
        }
    }
}