/* Copyright (C) 2020 Jack Zhang (jackz314) - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license */

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>    // std::find

//for directory related stuff, need C++ 17
#include <filesystem>

using namespace std;

bool VERBOSE = false;
bool OVERWRITE = true;

bool createDirIfNotExist(const string& pathToCreate){
    //create output directory if it doesn't exist
    filesystem::path dir(pathToCreate);
    if(!filesystem::exists(dir)){
        if(!filesystem::create_directory(dir)){
            cout << "Failed to create directory: " << pathToCreate << endl;
            return false;
        }
    }
    return true;
}

void displayHelp(){
    cout << "Usage: bars-to-bwav <bars file or folder containing bars files> [bwav output folder] [--no-overwrite]" << endl;
    cout << "or: `bars-to-bwav -h` to bring out this menu." << endl;
    cout << "add --no-overwrite flag to prevent from overwriting files with the same names." << endl;
}

int main(int argc, char const *argv[])
{
    if(argc < 2){
        cout << "Missing file arguments!" << endl;
        displayHelp();
        return -1;
    }

    if(argc == 2 && string(argv[1]) == "-h"){
        displayHelp();
        return 0;
    }

    vector<string> barsFiles;
    if(filesystem::is_directory(argv[1])){// process a directory of bars files
        filesystem::directory_iterator end_itr;
        filesystem::path p(argv[1]);
        cout << "BARS file to be processed in directory " << argv[1] << ':' << endl;
        for(filesystem::directory_iterator itr(p); itr != end_itr; itr++){
            //only deal with bars files
            if (is_regular_file(itr->path()) && itr->path().extension().generic_string() == ".bars") {
                string fileName = itr->path().string();
                barsFiles.push_back(fileName);
                cout << fileName << endl;
            }
        }

    }else{// process single file
        barsFiles.push_back(argv[1]);
        cout << "BARS file to be processed: " << argv[1] << endl;
    }

    string baseOutputDir;
    if(argc > 2){// have output folder or no overwrite
        if(argc == 4 && string(argv[3]) == "--no-overwrite"){ // have no overwrite and output folder
            baseOutputDir = argv[2];
            OVERWRITE = false;
        }
        if(argc == 3 && string(argv[2]) == "--no-overwrite"){ // only no overwrite
            baseOutputDir = "BWAV-Output/";
            OVERWRITE = false;
        }else{ // only out dir
            cout << '['<<argv[2]<<']' << endl;
            baseOutputDir = argv[2];
        }
        if(baseOutputDir.back() != '/' || baseOutputDir.back() != '\\'){
            baseOutputDir += '/';
        }
    }else{
        baseOutputDir = "BWAV-Output/";
    }

    string spliter = "----------------------------------------";
    
    int totalBWAVCount = 0;

    int countB = 0;
    for(string barsFile : barsFiles){
        ifstream inFile(barsFile, ios::binary);
        if(!inFile){
            cout << "Failed to open BARS file " << barsFile << endl;
            return -2;
        }
        inFile.seekg(0, ios::end);
        streamsize barsSize = inFile.tellg();
        inFile.seekg(0,ios::beg);
        
        cout << spliter << '(' << dec << ++countB << '/' << barsFiles.size() << ')' << spliter << endl;
        cout << "Processing BARS file " << barsFile << " size: " << barsSize << endl;

        vector<char> fBuf(barsSize);
        if(!inFile.read(fBuf.data(), barsSize)){
            //read filed
            cout << "Read file failed." << endl;
        }

        vector<string> audioNames;
        vector<streamoff> startOffsets;

        for (streamoff i = 0; i < barsSize - 4; i+=4){
            //Hex representation of BWAV's header start (BWAV: 0x42 0x57 0x41 0x56)
            if(fBuf[i] == 0x42 && fBuf[i+1] == 0x57 && fBuf[i+2] == 0x41 && fBuf[i+3] == 0x56){
                //matched BWAV, send to start offsets
                startOffsets.push_back(i);
               if(VERBOSE) cout << "Found BWAV at offset 0x" << hex << i <<endl;
            }

            /**
             * NorthWestWind's note:
             * the idea is to read backwards from 2nd AMTA and/or BWAV
             */
            //Hex representation of BARS's AMTA header start (AMTA: 0x41 0x4D 0x54 0x41)
            if(fBuf[i] == 0x41 && fBuf[i+1] == 0x4D && fBuf[i+2] == 0x54 && fBuf[i+3] == 0x41){
                if (VERBOSE) cout << "Found AMTA tag at offset 0x" << hex << i << endl;
                //matched AMTA, send to file names
                streamsize nameLen = 0;
                streamoff nameStart;
                // go to end of AMTA
                for (streamoff j = i + 4; j < barsSize - 4; j += 4) {
                    // find next tag
                    if (fBuf[j] == 0x41 && fBuf[j+1] == 0x4D && fBuf[j+2] == 0x54 && fBuf[j+3] == 0x41 || // AMTA
                        fBuf[j] == 0x42 && fBuf[j+1] == 0x57 && fBuf[j+2] == 0x41 && fBuf[j+3] == 0x56) { // BWAV
                        if (VERBOSE) cout << "Found next tag at offset 0x" << hex << j <<endl;
                        // now read backwards
                        bool foundName = false;
                        streamoff k = j-1;
                        while (fBuf[k] != '\0' || !foundName) {
                            if (fBuf[k] != '\0') {
                                nameStart = k;
                                nameLen++;
                                foundName = true;
                            }
                            k--;
                        }
                        break;
                    }
                }
                string name = string(fBuf.begin() + nameStart, fBuf.begin() + nameStart + nameLen);
                if(find(audioNames.begin(),audioNames.end(), name) != audioNames.end()){//already exists
                    //deal with duplicates with repeat counter
                    int repeatCounter = 1;
                    while(find(audioNames.begin(),audioNames.end(), name + '-' + to_string(repeatCounter)) != audioNames.end()){
                        repeatCounter++;
                    }
                    name += '-' + to_string(repeatCounter);
                }
                audioNames.push_back(name);
                if (VERBOSE) cout << "Name: " << name << endl;
            }
        }

        cout << endl << "Found all BWAV files. Total count: " << dec << startOffsets.size() << ". Writing bwav files to " << baseOutputDir << endl;
        
        if(!createDirIfNotExist(baseOutputDir)) return -3;

        if(audioNames.size() < startOffsets.size()){
            cout << "BWAV names count is not the same as BWAV counts!" << endl;
            for(size_t i = 1; i < startOffsets.size() - audioNames.size() + 1; i++){
                audioNames.push_back("extra_" + to_string(i));
            }
        }

        //write bwav files
        string onlyBarsFileName = filesystem::path(barsFile).filename().string();
        string outputDir = onlyBarsFileName.substr(0, onlyBarsFileName.size() - 5) + '/';//remove the extension and add directory slash
        cout << "Output subdirectory: " << outputDir << endl;
        if(!createDirIfNotExist(baseOutputDir + outputDir)) return -3;
        int illegal_counter = 0;
        for(size_t i = 0; i < startOffsets.size(); i++){
            streamoff offset = startOffsets[i];
            streamoff nextOffset;
            if(i+1 == startOffsets.size()){
                nextOffset = barsSize;
            }else{
                nextOffset = startOffsets[i+1];            
            }
            streamsize writeSize = nextOffset - offset;
            string fName = audioNames[i] + ".bwav";
            if(VERBOSE) cout << dec << '(' << i+1 << '/' << startOffsets.size() << ") Writing " << fName << " from offset 0x" << hex << offset << endl;
            string oFilePath = baseOutputDir + outputDir + fName;

            //deal with duplicate file names, already dealt with before, here to deal with pre-existing ones
            if(!OVERWRITE && filesystem::exists(oFilePath)){//don't overwrite
                int repeatCounter = 1;
                string newOFilePath = oFilePath.substr(0,oFilePath.size()-5) + '-';
                while(filesystem::exists(newOFilePath + to_string(repeatCounter) + ".bwav")){//already exists, use another name
                    repeatCounter++;
                }
                oFilePath = newOFilePath + to_string(repeatCounter) + ".bwav";
            }

            fstream oFile(oFilePath, ios::out | ios::binary);
            if(!oFile.write(fBuf.data()+offset, writeSize)){// if failed, probably due to illegal name
                ++illegal_counter;
                oFilePath = baseOutputDir + outputDir + "illegal_name_" + to_string(illegal_counter) + ".bwav";
                fstream oFileRetry(oFilePath, ios::out | ios::binary); // try again
                if(!oFileRetry.write(fBuf.data()+offset, writeSize)){ // still failed
                    cout << "Write bwav failed. Path: [" << oFilePath << "] Count: " << dec << i << " Offset: 0x" << hex << offset << endl;
                }else{
                    ++totalBWAVCount;
                }
                oFileRetry.close();
            }else{
                ++totalBWAVCount;
            }
            oFile.close();
        }
    }

    cout << spliter << spliter << endl;
    cout << "Done! Processed " << dec << barsFiles.size() << " BARS files and generated " << totalBWAVCount << " BWAV files in total." << endl;

    return 0;
}
