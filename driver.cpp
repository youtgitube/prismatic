#include "ArrayND.h"
#include "params.h"
#include "meta.h"
#include "PRISM_entry.h"

using namespace std;
int main(int argc, const char** argv) {
	using PRISM_FLOAT_TYPE = double;
	PRISM::Metadata<PRISM_FLOAT_TYPE> prism_meta;
	prism_meta.interpolationFactor = (argc>2) ? (size_t)atoi(argv[2]) : 50;


	PRISM_FLOAT_TYPE one_pixel_size = 100.0 / 1000.0;
	prism_meta.potBound = 1.0;
	prism_meta.numFP = 8.0 / 8.0;
	prism_meta.sliceThickness = 2;
	prism_meta.cellDim = vector<size_t>{80,100,100}; // this is z,y,x format
	prism_meta.E0 = 80e3;
	prism_meta.alphaBeamMax = 24 / 1000.0;
	prism_meta.NUM_GPUS = 1;
	prism_meta.NUM_THREADS = 12;

	std::string filename;
	if (argc>1) {
		prism_meta.filename_atoms = std::string(argv[1]);
	} else{
		cout << "PRISM: Correct syntax is prism filename [interpolation factor]" << endl;
		return 0;
	}
	prism_meta.filename_output = "prism_image.mrc";
	return PRISM_entry(prism_meta);
}
