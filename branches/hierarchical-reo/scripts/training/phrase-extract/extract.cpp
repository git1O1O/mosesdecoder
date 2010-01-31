/*
 * extract.cpp
 *
 *      Modified by: Nadi Tomeh - LIMSI/CNRS
 *      Machine Translation Marathon 2010, Dublin
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <cstring>

#include <map>
#include <set>
#include <vector>

using namespace std;

#define SAFE_GETLINE(_IS, _LINE, _SIZE, _DELIM) { \
                _IS.getline(_LINE, _SIZE, _DELIM); \
                if(_IS.fail() && !_IS.bad() && !_IS.eof()) _IS.clear(); \
                if (_IS.gcount() == _SIZE-1) { \
                  cerr << "Line too long! Buffer overflow. Delete lines >=" \
                    << _SIZE << " chars or raise LINE_MAX_LENGTH in phrase-extract/extract.cpp" \
                    << endl; \
                    exit(1); \
                } \
              }
#define LINE_MAX_LENGTH 60000

// HPhraseVertex represents a point in the alignment matrix
typedef pair <int, int> HPhraseVertex;

// Phrase represents a bi-phrase; each bi-phrase is defined by two points in the alignment matrix:
// bottom-left and top-right
typedef pair<HPhraseVertex, HPhraseVertex> HPhrase;

// HPhraseVector is a vector of HPhrases
typedef vector < HPhrase > HPhraseVector;

// SentenceVertices represents, from all extracted phrases, all vertices that have the same positioning
// The key of the map is the English index and the value is a set of the foreign ones
typedef map <int, set<int> > HSenteceVertices;

enum REO_MODEL_TYPE {REO_MSD, REO_MSLR, REO_MONO};
enum REO_POS {LEFT, RIGHT, DLEFT, DRIGHT, UNKNOWN};

class SentenceAlignment {
	public:
		vector<string> english;
		vector<string> foreign;
		vector<int> alignedCountF;
		vector< vector<int> > alignedToE;

		int create( char[], char[], char[], int );
		//  void clear() { delete(alignment); };
};

REO_POS getOrientWordModel(SentenceAlignment &, REO_MODEL_TYPE,
		int, int, int, int, int, int, int,
		bool (*)(int, int), bool (*)(int, int));
REO_POS getOrientPhraseModel(REO_MODEL_TYPE,
		int, int, int, int, int, int, int,
		bool (*)(int, int), bool (*)(int, int),
		const HSenteceVertices &, const HSenteceVertices &);
REO_POS getOrientHierModel(REO_MODEL_TYPE,
		int, int, int, int, int, int, int,
		bool (*)(int, int), bool (*)(int, int),
		const HSenteceVertices &, const HSenteceVertices &,
		REO_POS);

void insertVertex(HSenteceVertices &, int, int);
void insertPhraseVertices(HSenteceVertices &, HSenteceVertices &, HSenteceVertices &, HSenteceVertices &,
		int, int, int, int);
string getOrientString(REO_POS, REO_MODEL_TYPE);

bool ge(int, int);
bool le(int, int);
bool lt(int, int);

void extractBase(SentenceAlignment &);
void extract(SentenceAlignment &);
void addPhrase(SentenceAlignment &, int, int, int, int, string &);
vector<string> tokenize(char []);
bool isAligned (SentenceAlignment &, int, int);

bool allModelsOutputFlag = false;

bool wordModel = false;
REO_MODEL_TYPE wordType = REO_MSD;
bool phraseModel = false;
REO_MODEL_TYPE phraseType = REO_MSD;
bool hierModel = false;
REO_MODEL_TYPE hierType = REO_MSD;


ofstream extractFile;
ofstream extractFileInv;
ofstream extractFileOrientation;
int maxPhraseLength;
int phraseCount = 0;
char* fileNameExtract;
bool orientationFlag = false;
bool onlyOutputSpanInfo = false;
bool noFileLimit = false;
bool zipFiles = false;
bool properConditioning = false;

int main(int argc, char* argv[])
{
	cerr	<< "PhraseExtract v1.4, written by Philipp Koehn\n"
			<< "phrase extraction from an aligned parallel corpus\n";
	time_t starttime = time(NULL);

	if (argc < 6) {
		cerr << "syntax: extract en de align extract max-length [orientation [ --model [wbe|phrase|hier]-[msd|mslr|mono] ] | --OnlyOutputSpanInfo | --NoFileLimit | --ProperConditioning]\n";
		exit(1);
	}
	char* &fileNameE = argv[1];
	char* &fileNameF = argv[2];
	char* &fileNameA = argv[3];
	fileNameExtract = argv[4];
	maxPhraseLength = atoi(argv[5]);

	for(int i=6;i<argc;i++) {
		if (strcmp(argv[i],"--OnlyOutputSpanInfo") == 0) {
			onlyOutputSpanInfo = true;
		}
		else if (strcmp(argv[i],"--NoFileLimit") == 0) {
			noFileLimit = true;
		}
		else if (strcmp(argv[i],"orientation") == 0 || strcmp(argv[i],"--Orientation") == 0) {
			orientationFlag = true;
		}
		else if(strcmp(argv[i],"--model") == 0){
			if (i+1 >= argc){
				cerr << "extract: syntax error, no model's information provided to the option --model " << endl;
				exit(1);
			}
			char* modelParams = argv[++i];
			char* modelName = strtok(modelParams, "-");
			char* modelType = strtok(NULL, "-");

			REO_MODEL_TYPE intModelType;

			if(strcmp(modelName, "wbe") == 0){
				wordModel = true;
				if(strcmp(modelType, "msd") == 0)
					wordType = REO_MSD;
				else if(strcmp(modelType, "mslr") == 0)
					wordType = REO_MSLR;
				else if(strcmp(modelType, "mono") == 0)
					wordType = REO_MONO;
				else{
					cerr << "extract: syntax error, unknown reordering model type: " << modelType << endl;
					exit(1);
				}
			}
			else if(strcmp(modelName, "phrase") == 0){
				phraseModel = true;
				if(strcmp(modelType, "msd") == 0)
					phraseType = REO_MSD;
				else if(strcmp(modelType, "mslr") == 0)
					phraseType = REO_MSLR;
				else if(strcmp(modelType, "mono") == 0)
					phraseType = REO_MONO;
				else{
					cerr << "extract: syntax error, unknown reordering model type: " << modelType << endl;
					exit(1);
				}
			}
			else if(strcmp(modelName, "hier") == 0){
				hierModel = true;
				if(strcmp(modelType, "msd") == 0)
					hierType = REO_MSD;
				else if(strcmp(modelType, "mslr") == 0)
					hierType = REO_MSLR;
				else if(strcmp(modelType, "mono") == 0)
					hierType = REO_MONO;
				else{
					cerr << "extract: syntax error, unknown reordering model type: " << modelType << endl;
					exit(1);
				}
			}
			else{
				cerr << "extract: syntax error, unknown reordering model: " << modelName << endl;
				exit(1);
			}

			allModelsOutputFlag = true;
		}
		else if (strcmp(argv[i],"--ZipFiles") == 0) {
			zipFiles = true;
		}
		else if (strcmp(argv[i],"--ProperConditioning") == 0) {
			properConditioning = true;
		}
		else {
			cerr << "extract: syntax error, unknown option '" << string(argv[i]) << "'\n";
			exit(1);
		}
	}

	// default reordreing model if no model selected
	// allows for the old syntax to be used
	if(orientationFlag && !allModelsOutputFlag){
		wordModel = true;
		wordType = REO_MSD;
	}

	ifstream eFile;
	ifstream fFile;
	ifstream aFile;
	eFile.open(fileNameE);
	fFile.open(fileNameF);
	aFile.open(fileNameA);
	istream *eFileP = &eFile;
	istream *fFileP = &fFile;istream *aFileP = &aFile;

	int i=0;
	while(true) {
		i++;
		if (i%10000 == 0) cerr << "." << flush;
		char englishString[LINE_MAX_LENGTH];
		char foreignString[LINE_MAX_LENGTH];
		char alignmentString[LINE_MAX_LENGTH];
		SAFE_GETLINE((*eFileP), englishString, LINE_MAX_LENGTH, '\n');
		if (eFileP->eof()) break;
		SAFE_GETLINE((*fFileP), foreignString, LINE_MAX_LENGTH, '\n');
		SAFE_GETLINE((*aFileP), alignmentString, LINE_MAX_LENGTH, '\n');
		SentenceAlignment sentence;
		// cout << "read in: " << englishString << " & " << foreignString << " & " << alignmentString << endl;
		//az: output src, tgt, and alingment line
		if (onlyOutputSpanInfo) {
			cout << "LOG: SRC: " << foreignString << endl;
			cout << "LOG: TGT: " << englishString << endl;
			cout << "LOG: ALT: " << alignmentString << endl;
			cout << "LOG: PHRASES_BEGIN:" << endl;
		}

		if (sentence.create( englishString, foreignString, alignmentString, i )) {
			extract(sentence);
		}
		if (onlyOutputSpanInfo) cout << "LOG: PHRASES_END:" << endl; //az: mark end of phrases
	}
	eFile.close();
	fFile.close();
	aFile.close();
	//az: only close if we actually opened it
	if (!onlyOutputSpanInfo) {
		extractFile.close();
		extractFileInv.close();
		if (orientationFlag) extractFileOrientation.close();
	}
}

void extract(SentenceAlignment &sentence) {
	int countE = sentence.english.size();
	int countF = sentence.foreign.size();

	HPhraseVector inboundPhrases;

	HSenteceVertices inTopLeft;
	HSenteceVertices inTopRight;
	HSenteceVertices inBottomLeft;
	HSenteceVertices inBottomRight;

	HSenteceVertices outTopLeft;
	HSenteceVertices outTopRight;
	HSenteceVertices outBottomLeft;
	HSenteceVertices outBottomRight;

	HSenteceVertices::const_iterator it;

	bool relaxLimit = hierModel;
	bool buildExtraStructure = phraseModel || hierModel;

	// check alignments for english phrase startE...endE
	// loop over extracted phrases which are compatible with the word-alignments
	for(int startE=0;startE<countE;startE++) {
		for(int endE=startE;
				(endE<countE && (relaxLimit || endE<startE+maxPhraseLength));
				endE++) {

			int minF = 9999;
			int maxF = -1;
			vector< int > usedF = sentence.alignedCountF;
			for(int ei=startE;ei<=endE;ei++) {
				for(int i=0;i<sentence.alignedToE[ei].size();i++) {
					int fi = sentence.alignedToE[ei][i];
					if (fi<minF) { minF = fi; }
					if (fi>maxF) { maxF = fi; }
					usedF[ fi ]--;
				}
			}

			if (maxF >= 0 && // aligned to any foreign words at all
					(relaxLimit || maxF-minF < maxPhraseLength)) { // foreign phrase within limits

				// check if foreign words are aligned to out of bound english words
				bool out_of_bounds = false;
				for(int fi=minF;fi<=maxF && !out_of_bounds;fi++)
					if (usedF[fi]>0) {
						// cout << "ouf of bounds: " << fi << "\n";
						out_of_bounds = true;
					}

				// cout << "doing if for ( " << minF << "-" << maxF << ", " << startE << "," << endE << ")\n";
				if (!out_of_bounds){
					// start point of foreign phrase may retreat over unaligned
					for(int startF=minF;
							(startF>=0 &&
									(relaxLimit || startF>maxF-maxPhraseLength) && // within length limit
									(startF==minF || sentence.alignedCountF[startF]==0)); // unaligned
							startF--)
						// end point of foreign phrase may advance over unaligned
						for(int endF=maxF;
								(endF<countF &&
										(relaxLimit || endF<startF+maxPhraseLength) && // within length limit
										(endF==maxF || sentence.alignedCountF[endF]==0)); // unaligned
								endF++){ // at this point we have extracted a phrase
							if(buildExtraStructure){ // phrase || hier
								if(endE-startE < maxPhraseLength && endF-startF < maxPhraseLength){ // within limit
									inboundPhrases.push_back(
										HPhrase(
												HPhraseVertex(startF,startE),
												HPhraseVertex(endF,endE)
												)
										);
										insertPhraseVertices(inTopLeft, inTopRight, inBottomLeft, inBottomRight,
													startF, startE, endF, endE);
								}
								else
									insertPhraseVertices(outTopLeft, outTopRight, outBottomLeft, outBottomRight,
											startF, startE, endF, endE);
							}
							else{
								string orientationInfo = "";
								if(wordModel){
									REO_POS wordPrevOrient, wordNextOrient;
									wordPrevOrient = getOrientWordModel(sentence, wordType, startF, endF, startE, endE, countF, 0, 1, &ge, &lt);
									wordNextOrient = getOrientWordModel(sentence, wordType, endF, startF, endE, startE, 0, countF, -1, &lt, &ge);
									if(allModelsOutputFlag)
										orientationInfo += getOrientString(wordPrevOrient, wordType) + " " + getOrientString(wordNextOrient, wordType) + "| | ";
									else
										orientationInfo += getOrientString(wordPrevOrient, wordType) + " " + getOrientString(wordNextOrient, wordType);
								}
								addPhrase(sentence, startE, endE, startF, endF, orientationInfo);
							}
						}
					}
				}
			}
	}

	if(buildExtraStructure){ // phrase || hier
		string orientationInfo = "";
		REO_POS wordPrevOrient, wordNextOrient, phrasePrevOrient, phraseNextOrient, hierPrevOrient, hierNextOrient;

		for(int i = 0; i < inboundPhrases.size(); i++){
			int startF = inboundPhrases[i].first.first;
			int startE = inboundPhrases[i].first.second;
			int endF = inboundPhrases[i].second.first;
			int endE = inboundPhrases[i].second.second;

			if(wordModel){
				wordPrevOrient = getOrientWordModel(sentence, wordType,
						startF, endF, startE, endE, countF, 0, 1,
						&ge, &lt);
				wordNextOrient = getOrientWordModel(sentence, wordType,
						endF, startF, endE, startE, 0, countF, -1,
						&lt, &ge);
			}
			phrasePrevOrient = getOrientPhraseModel(phraseType, startF, endF, startE, endE, countF-1, 0, 1, &ge, &lt, inBottomRight, inBottomLeft);
			phraseNextOrient = getOrientPhraseModel(phraseType, endF, startF, endE, startE, 0, countF-1, -1, &lt, &ge, inBottomLeft, inBottomRight);
			if(hierModel){
				hierPrevOrient = getOrientHierModel(phraseType, startF, endF, startE, endE, countF-1, 0, 1, &ge, &lt, outBottomRight, outBottomLeft, phrasePrevOrient);
				hierNextOrient = getOrientHierModel(phraseType, endF, startF, endE, startE, 0, countF-1, -1, &lt, &ge, outBottomLeft, outBottomRight, phraseNextOrient);
			}

			orientationInfo = ((wordModel)? getOrientString(wordPrevOrient, wordType) + " " + getOrientString(wordNextOrient, wordType) : " ") + "|" +
								((phraseModel)? getOrientString(phrasePrevOrient, phraseType) + " " + getOrientString(phraseNextOrient, phraseType) : " ") + "|" +
								((hierModel)? getOrientString(hierPrevOrient, hierType) + " " + getOrientString(hierNextOrient, hierType) : " ");

			addPhrase(sentence, startE, endE, startF, endF, orientationInfo);
		}
	}
}

REO_POS getOrientWordModel(SentenceAlignment & sentence, REO_MODEL_TYPE modelType,
		int startF, int endF, int startE, int endE, int countF, int zero, int unit,
		bool (*ge)(int, int), bool (*lt)(int, int) ){

	bool connectedLeftTop  = isAligned( sentence, startF-unit, startE-unit );
	bool connectedRightTop = isAligned( sentence, endF+unit,   startE-unit );
	if( connectedLeftTop && !connectedRightTop)
		return LEFT;
	if(modelType == REO_MONO)
		return UNKNOWN;
	if (!connectedLeftTop &&  connectedRightTop)
		return RIGHT;
	if(modelType == REO_MSD)
		return UNKNOWN;
	for(int indexF=startF-2*unit; (*ge)(indexF, zero) && !connectedLeftTop; indexF=indexF-unit)
		connectedLeftTop = isAligned(sentence, indexF, startE-unit);
	for(int indexF=endF+2*unit; (*lt)(indexF,countF) && !connectedRightTop; indexF=indexF+unit)
		connectedRightTop = isAligned(sentence, indexF, startE-unit);
	if(connectedLeftTop && !connectedRightTop)
		return DRIGHT;
	else if(!connectedLeftTop && connectedRightTop)
		return DLEFT;
	return UNKNOWN;
}

// to be called with countF-1 instead of countF
REO_POS getOrientPhraseModel (REO_MODEL_TYPE modelType,
		int startF, int endF, int startE, int endE, int countF, int zero, int unit,
		bool (*ge)(int, int), bool (*le)(int, int),
		const HSenteceVertices & inBottomRight, const HSenteceVertices & inBottomLeft){

	HSenteceVertices::const_iterator it;

	if((startE == zero && startF == zero) ||
			(it = inBottomRight.find(startE - unit)) != inBottomRight.end() &&
			it->second.find(startF-unit) != it->second.end())
		return LEFT;
	if(modelType == REO_MONO)
		return UNKNOWN;
	if((it = inBottomLeft.find(startE - unit)) != inBottomLeft.end() && it->second.find(endF + unit) != it->second.end())
		return RIGHT;
	if(modelType == REO_MSD)
		return UNKNOWN;
	bool connectedLeftTop = false;
	for(int indexF=startF-2*unit; (*ge)(indexF, zero) && !connectedLeftTop; indexF=indexF-unit)
		if(connectedLeftTop = (it = inBottomRight.find(startE - unit)) != inBottomRight.end() &&
				it->second.find(indexF) != it->second.end())
			return DRIGHT;
	bool connectedRightTop = false;
	for(int indexF=endF+2*unit; (*le)(indexF, countF) && !connectedRightTop; indexF=indexF+unit)
		if(connectedRightTop = (it = inBottomLeft.find(startE - unit)) != inBottomRight.end() &&
				it->second.find(indexF) != it->second.end())
			return DLEFT;
	return DRIGHT;
}

// to be called with countF-1 instead of countF
REO_POS getOrientHierModel (REO_MODEL_TYPE modelType,
		int startF, int endF, int startE, int endE, int countF, int zero, int unit,
		bool (*ge)(int, int), bool (*le)(int, int),
		const HSenteceVertices & outBottomRight, const HSenteceVertices & outBottomLeft,
		REO_POS phraseOrient){

	HSenteceVertices::const_iterator it;

	if(phraseOrient == LEFT || ((it = outBottomRight.find(startE - unit)) != outBottomRight.end() &&
			it->second.find(startF-unit) != it->second.end()))
		return LEFT;
	if(modelType == REO_MONO)
		return UNKNOWN;
	if(phraseOrient == RIGHT || ((it = outBottomLeft.find(startE - unit)) != outBottomLeft.end() && it->second.find(endF + unit) != it->second.end()))
		return RIGHT;
	if(modelType == REO_MSD)
		return UNKNOWN;
	if(phraseOrient == DRIGHT)
		return DRIGHT;
	if(phraseOrient == DLEFT)
		return DLEFT;
	bool connectedLeftTop = false;
	for(int indexF=startF-2*unit; (*ge)(indexF, zero) && !connectedLeftTop; indexF=indexF-unit)
		if(connectedLeftTop = (it = outBottomRight.find(startE - unit)) != outBottomRight.end() &&
				it->second.find(indexF) != it->second.end())
			return DRIGHT;
	bool connectedRightTop = false;
	for(int indexF=endF+2*unit; (*le)(indexF, countF) && !connectedRightTop; indexF=indexF+unit)
		if(connectedRightTop = (it = outBottomLeft.find(startE - unit)) != outBottomRight.end() &&
				it->second.find(indexF) != it->second.end())
			return DLEFT;
	return UNKNOWN;
}

bool isAligned ( SentenceAlignment &sentence, int fi, int ei ){
	if (ei == -1 && fi == -1)
		return true;
	if (ei <= -1 || fi <= -1)
		return false;
	if (ei == sentence.english.size() && fi == sentence.foreign.size())
		return true;
	if (ei >= sentence.english.size() || fi >= sentence.foreign.size())
		return false;
	for(int i=0;i<sentence.alignedToE[ei].size();i++)
		if (sentence.alignedToE[ei][i] == fi)
			return true;
	return false;
}

bool ge(int first, int second){
	return first >= second;
}

bool le(int first, int second){
	return first <= second;
}

bool lt(int first, int second){
	return first < second;
}

void insertVertex( HSenteceVertices & corners, int x, int y ){
	set<int> tmp;
	tmp.insert(x);
	pair< HSenteceVertices::iterator, bool > ret = corners.insert( pair<int, set<int> > (y, tmp) );
	if(ret.second == false){
		ret.first->second.insert(x);
	}
}

void insertPhraseVertices(
		HSenteceVertices & topLeft,
		HSenteceVertices & topRight,
		HSenteceVertices & bottomLeft,
		HSenteceVertices & bottomRight,
		int startF, int startE, int endF, int endE) {

	insertVertex(topLeft, startF, startE);
	insertVertex(topRight, endF, startE);
	insertVertex(bottomLeft, startF, endE);
	insertVertex(bottomRight, endF, endE);
}

string getOrientString(REO_POS orient, REO_MODEL_TYPE modelType){
	switch(orient){
	case LEFT: return "mono"; break;
	case RIGHT: return "swap"; break;
	case DRIGHT: return "dright"; break;
	case DLEFT: return "dleft"; break;
	case UNKNOWN:
		switch(modelType){
		case REO_MONO: return "nomono"; break;
		case REO_MSD: return "other"; break;
		case REO_MSLR: return "dright"; break;
		}
		break;
	}
}

void addPhrase( SentenceAlignment &sentence, int startE, int endE, int startF, int endF , string &orientationInfo) {
  // foreign
  // cout << "adding ( " << startF << "-" << endF << ", " << startE << "-" << endE << ")\n";

  if (onlyOutputSpanInfo) {
    cout << startF << " " << endF << " " << startE << " " << endE << endl;
    return;
  }

  // new file every 1e7 phrases
  if (phraseCount % 10000000 == 0 // new file every 1e7 phrases
      && (!noFileLimit || phraseCount == 0)) { // only new partial file, if file limit

    // close old file
    if (!noFileLimit && phraseCount>0) {
      extractFile.close();
      extractFileInv.close();
      if (orientationFlag) extractFileOrientation.close();
    }

    // construct file name
    char part[10];
    if (noFileLimit)
      part[0] = '\0';
    else
      sprintf(part,".part%04d",phraseCount/10000000);
    string fileNameExtractPart = string(fileNameExtract) + part;
    string fileNameExtractInvPart = string(fileNameExtract) + ".inv" + part;
    string fileNameExtractOrientationPart = string(fileNameExtract) + ".o" + part;


    // open files
    extractFile.open(fileNameExtractPart.c_str());
    extractFileInv.open(fileNameExtractInvPart.c_str());
    if (orientationFlag)
      extractFileOrientation.open(fileNameExtractOrientationPart.c_str());
  }


  phraseCount++;

  for(int fi=startF;fi<=endF;fi++) {
    extractFile << sentence.foreign[fi] << " ";
    if (orientationFlag) extractFileOrientation << sentence.foreign[fi] << " ";
  }
  extractFile << "||| ";
  if (orientationFlag) extractFileOrientation << "||| ";

  // english
  for(int ei=startE;ei<=endE;ei++) {
    extractFile << sentence.english[ei] << " ";
    extractFileInv << sentence.english[ei] << " ";
    if (orientationFlag) extractFileOrientation << sentence.english[ei] << " ";
  }
  extractFile << "|||";
  extractFileInv << "||| ";
  if (orientationFlag) extractFileOrientation << "||| ";

  // foreign (for inverse)
  for(int fi=startF;fi<=endF;fi++)
    extractFileInv << sentence.foreign[fi] << " ";
  extractFileInv << "|||";

  // alignment
  for(int ei=startE;ei<=endE;ei++)
    for(int i=0;i<sentence.alignedToE[ei].size();i++) {
      int fi = sentence.alignedToE[ei][i];
      extractFile << " " << fi-startF << "-" << ei-startE;
      extractFileInv << " " << ei-startE << "-" << fi-startF;
    }

  if (orientationFlag)
      extractFileOrientation << orientationInfo;

  extractFile << "\n";
  extractFileInv << "\n";
  if (orientationFlag) extractFileOrientation << "\n";
}

// if proper conditioning, we need the number of times a foreign phrase occured
void extractBase( SentenceAlignment &sentence ) {
  int countF = sentence.foreign.size();
  for(int startF=0;startF<countF;startF++) {
    for(int endF=startF;
        (endF<countF && endF<startF+maxPhraseLength);
        endF++) {
      for(int fi=startF;fi<=endF;fi++) {
	extractFile << sentence.foreign[fi] << " ";
      }
      extractFile << "|||" << endl;
    }
  }

  int countE = sentence.english.size();
  for(int startE=0;startE<countE;startE++) {
    for(int endE=startE;
        (endE<countE && endE<startE+maxPhraseLength);
        endE++) {
      for(int ei=startE;ei<=endE;ei++) {
	extractFileInv << sentence.english[ei] << " ";
      }
      extractFileInv << "|||" << endl;
    }
  }
}

int SentenceAlignment::create( char englishString[], char foreignString[], char alignmentString[], int sentenceID ) {
  english = tokenize( englishString );
  foreign = tokenize( foreignString );
  //  alignment = new bool[foreign.size()*english.size()];
  //  alignment = (bool**) calloc(english.size()*foreign.size(),sizeof(bool)); // is this right?

  if (english.size() == 0 || foreign.size() == 0) {
    cerr << "no english (" << english.size() << ") or foreign (" << foreign.size() << ") words << end insentence " << sentenceID << endl;
    cerr << "E: " << englishString << endl << "F: " << foreignString << endl;
    return 0;
  }
  // cout << "english.size = " << english.size() << endl;
  // cout << "foreign.size = " << foreign.size() << endl;

  // cout << "xxx\n";
  for(int i=0; i<foreign.size(); i++) {
    // cout << "i" << i << endl;
    alignedCountF.push_back( 0 );
  }
  for(int i=0; i<english.size(); i++) {
    vector< int > dummy;
    alignedToE.push_back( dummy );
  }
  // cout << "\nscanning...\n";

  vector<string> alignmentSequence = tokenize( alignmentString );
  for(int i=0; i<alignmentSequence.size(); i++) {
    int e,f;
    // cout << "scaning " << alignmentSequence[i].c_str() << endl;
    if (! sscanf(alignmentSequence[i].c_str(), "%d-%d", &f, &e)) {
      cerr << "WARNING: " << alignmentSequence[i] << " is a bad alignment point in sentnce " << sentenceID << endl;
      cerr << "E: " << englishString << endl << "F: " << foreignString << endl;
      return 0;
    }
      // cout << "alignmentSequence[i] " << alignmentSequence[i] << " is " << f << ", " << e << endl;
    if (e >= english.size() || f >= foreign.size()) {
      cerr << "WARNING: sentence " << sentenceID << " has alignment point (" << f << ", " << e << ") out of bounds (" << foreign.size() << ", " << english.size() << ")\n";
      cerr << "E: " << englishString << endl << "F: " << foreignString << endl;
      return 0;
    }
    alignedToE[e].push_back( f );
    alignedCountF[f]++;
  }
  return 1;
}
