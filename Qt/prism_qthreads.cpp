// Copyright Alan (AJ) Pryor, Jr. 2017
// Transcribed from MATLAB code by Colin Ophus
// Prismatic is distributed under the GNU General Public License (GPL)
// If you use Prismatic, we kindly ask that you cite the following papers:

// 1. Ophus, C.: A fast image simulation algorithm for scanning
//    transmission electron microscopy. Advanced Structural and
//    Chemical Imaging 3(1), 13 (2017)

// 2. Pryor, Jr., A., Ophus, C., and Miao, J.: A Streaming Multi-GPU
//    Implementation of Image Simulation Algorithms for Scanning
//	  Transmission Electron Microscopy. arXiv:1706.08563 (2017)

#include "prism_qthreads.h"
#include "prism_progressbar.h"
#include "PRISM01_calcPotential.h"
#include "PRISM02_calcSMatrix.h"
#include "PRISM03_calcOutput.h"
#include "Multislice_calcOutput.h"
#include "utility.h"
#include <iostream>
#include <string>
#include "H5Cpp.h"
#include "QMessageBox"
#include <stdio.h>

PRISMThread::PRISMThread(PRISMMainWindow *_parent, prism_progressbar *_progressbar) : parent(_parent), progressbar(_progressbar)
{
    // construct the thread with a copy of the metadata so that any upstream changes don't mess with this calculation
    QMutexLocker gatekeeper(&this->parent->dataLock);
    this->meta = *(parent->getMetadata());
}

PotentialThread::PotentialThread(PRISMMainWindow *_parent, prism_progressbar *_progressbar) : PRISMThread(_parent, _progressbar){};

void PotentialThread::run()
{
    // create parameters
    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
    //prevent it from trying to write to a non-existing H5 file
    params.meta.savePotentialSlices = false;
    progressbar->signalDescriptionMessage("Computing projected potential");

    {
        QMutexLocker calculationLocker(&this->parent->calculationLock);
        Prismatic::configure(meta);
        // calculate potential
        Prismatic::PRISM01_calcPotential(params);
        this->parent->potentialReceived(params.pot);
        emit potentialCalculated();
    }
    // acquire the mutex so we can safely copy to the GUI copy of the potential
    QMutexLocker gatekeeper(&this->parent->dataLock);
    // perform copy
    this->parent->pars = params;
    // indicate that the potential is ready

    // std::cout <<this->parent->saveProjectedPotential<<std::endl;
    /*
    if (this->parent->saveProjectedPotential){
      std::string outfile = Prismatic::remove_extension(params.meta.filenameOutput.c_str());
      outfile += ("_potential.mrc");
      std::cout<<"Outputting potential with filename "<<outfile<<std::endl;
      params.pot.toMRC_f(outfile.c_str());
    }
    */
    //    this->parent->potentialArrayExists = true;
    //    if (this->parent->saveProjectedPotential)params.pot.toMRC_f("potential.mrc");
    std::cout << "Projected potential calculation complete" << std::endl;
}

//SMatrixThread::SMatrixThread(PRISMMainWindow *_parent, prism_progressbar *_progressbar) :
//        parent(_parent), progressbar(_progressbar){
//    // construct the thread with a copy of the metadata so that any upstream changes don't mess with this calculation
//    QMutexLocker gatekeeper(&this->parent->dataLock);
//    this->meta = *(parent->getMetadata());
//}

//void SMatrixThread::run(){
//    QMutexLocker calculationLocker(&this->parent->calculationLock);
//    Prismatic::configure(meta);
//    // create parameters
//    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
//    // calculate potential if it hasn't been already
//    if (!this->parent->potentialIsReady()){
//        // calculate potential
//        Prismatic::PRISM01_calcPotential(params);
//	    QMutexLocker gatekeeper(&this->parent->dataLock);
//        // indicate that the potential is ready

////        this->parent->potentialArrayExists = true;
//        if (this->parent->saveProjectedPotential)params.pot.toMRC_f("potential.mrc");
//        this->parent->potentialReceived(params.pot);
//        emit potentialCalculated();
//    } else {
//        QMutexLocker gatekeeper(&this->parent->dataLock);
//        params = this->parent->pars;
//        params.progressbar = progressbar;
//        std::cout << "Potential already calculated. Using existing result." << std::endl;
//    }

////    // calculate S-Matrix
//    Prismatic::PRISM02_calcSMatrix(params);
//    QMutexLocker gatekeeper(&this->parent->dataLock);

////    // perform copy
//    this->parent->pars = params;
//    this->parent->ScompactReady = true;
//    std::cout << "S-matrix calculation complete" << std::endl;
//}

ProbeThread::ProbeThread(PRISMMainWindow *_parent, PRISMATIC_FLOAT_PRECISION _X, PRISMATIC_FLOAT_PRECISION _Y, prism_progressbar *_progressbar, bool _use_log_scale) : PRISMThread(_parent, _progressbar), X(_X), Y(_Y), use_log_scale(_use_log_scale){};

void ProbeThread::run()
{
    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
    progressbar->signalDescriptionMessage("Computing single probe");
    progressbar->setProgress(10);
    //    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta);

    //    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params_multi(params);
    params.meta.save4DOutput = false;
    params.meta.savePotentialSlices = false;
    params.meta.saveDPC_CoM = false;

    QMutexLocker calculationLocker(&this->parent->calculationLock);

    if ((!this->parent->potentialIsReady()) || !(params.meta == *(this->parent->getMetadata())))
    {
        //    if ((!this->parent->potentialIsReady())){

        Prismatic::configure(meta);
        this->parent->resetCalculation(); // any time we are computing the potential we are effectively starting over the whole calculation, so make sure all flags are reset
        Prismatic::PRISM01_calcPotential(params);
        std::cout << "Potential Calculated" << std::endl;
        {
            QMutexLocker gatekeeper(&this->parent->dataLock);
            this->parent->pars = params;
        }
        //            this->parent->potentialArrayExists = true;

        this->parent->potentialReceived(params.pot);
        emit potentialCalculated();
        /*
        if (this->parent->saveProjectedPotential){
          std::string outfile = Prismatic::remove_extension(params.meta.filenameOutput.c_str());
          outfile += ("_potential.mrc");
          std::cout<<"Outputting potential with filename "<<outfile<<std::endl;
          params.pot.toMRC_f(outfile.c_str());
        }
        */
    }
    else
    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        params = this->parent->pars;
        params.progressbar = progressbar;
        std::cout << "Potential already calculated. Using existing result." << std::endl;
    }

    if ((!this->parent->SMatrixIsReady()) || !(params.meta == *(this->parent->getMetadata())))
    {
        Prismatic::PRISM02_calcSMatrix(params);
        {
            std::cout << "S-Matrix finished calculating." << std::endl;
            QMutexLocker gatekeeper(&this->parent->dataLock);
            // perform copy
            this->parent->pars = params;

            // indicate that the S-Matrix is ready
            this->parent->ScompactReady = true;
        }
    }
    else
    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        params = this->parent->pars;
        params.progressbar = progressbar;
        std::cout << "S-Matrix already calculated. Using existing result." << std::endl;
    }

    std::pair<Prismatic::Array2D<std::complex<PRISMATIC_FLOAT_PRECISION>>, Prismatic::Array2D<std::complex<PRISMATIC_FLOAT_PRECISION>>> prism_probes, multislice_probes;
    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params_multi(params);
    params_multi.meta.save4DOutput = false;
    params_multi.meta.savePotentialSlices = false;
    params_multi.meta.saveDPC_CoM = false;

    // setup and calculate PRISM probe
    Prismatic::setupCoordinates_2(params);

    // setup angles of detector and image sizes
    Prismatic::setupDetector(params);

    // setup coordinates and indices for the beams
    Prismatic::setupBeams_2(params);

    // setup Fourier coordinates for the S-matrix
    Prismatic::setupFourierCoordinates(params);

    // initialize the output to the correct size for the output mode
    Prismatic::createStack_integrate(params);

    //  perform some necessary setup transformations of the data
    Prismatic::transformIndices(params);

    // initialize/compute the probes
    Prismatic::initializeProbes(params);

    std::cout << "Getting PRISM Probe" << std::endl;
    std::cout << "X = " << X << std::endl;
    std::cout << "Y = " << Y << std::endl;
    prism_probes = Prismatic::getSinglePRISMProbe_CPU(params, X, Y);

    // setup and calculate Multislice probe
    // setup coordinates and build propagators
    Prismatic::setupCoordinates_multislice(params_multi);

    // setup detector coordinates and angles
    Prismatic::setupDetector_multislice(params_multi);

    // create initial probes
    Prismatic::setupProbes_multislice(params_multi);

    // create transmission array
    Prismatic::createTransmission(params_multi);

    // initialize output stack
    Prismatic::createStack(params_multi);

    multislice_probes = Prismatic::getSingleMultisliceProbe_CPU(params_multi, X, Y);

    //    if (params.meta == *(this->parent->getMetadata())){
    //        std::cout << "Metadata equal!" << std::endl;
    //    } else {
    //        std::cout << "Metadata not equal!" << std::endl;
    //    }

    QMutexLocker gatekeeper(&this->parent->dataLock);
    // perform copy
    //    this->parent->pars = params;
    this->parent->probeSetupReady = true;

    prism_probes = upsamplePRISMProbe(prism_probes.first,
                                      multislice_probes.first.get_dimj(),
                                      multislice_probes.first.get_dimi(),
                                      Y / params.pixelSize[0] / 2,
                                      X / params.pixelSize[1] / 2);

    PRISMATIC_FLOAT_PRECISION pr_sum, pk_sum, mr_sum, mk_sum;
    pr_sum = pk_sum = mr_sum = mk_sum = 0;
    for (auto &i : prism_probes.first)
        pr_sum += abs(i);
    for (auto &i : prism_probes.second)
        pk_sum += abs(i);
    for (auto &i : multislice_probes.first)
        mr_sum += abs(i);
    for (auto &i : multislice_probes.second)
        mk_sum += abs(i);

    for (auto &i : prism_probes.first)
        i /= pr_sum;
    for (auto &i : prism_probes.second)
        i /= pk_sum;
    for (auto &i : multislice_probes.first)
        i /= mr_sum;
    for (auto &i : multislice_probes.second)
        i /= mk_sum;

    emit signal_pearsonReal(QString("Pearson Correlation = ") + QString::number(computePearsonCorrelation(prism_probes.first, multislice_probes.first)));
    emit signal_pearsonK(QString("Pearson Correlation = ") + QString::number(computePearsonCorrelation(prism_probes.second, multislice_probes.second)));
    emit signal_RReal(QString("R = ") + QString::number(computeRfactor(prism_probes.first, multislice_probes.first)));
    emit signal_RK(QString("R = ") + QString::number(computeRfactor(prism_probes.second, multislice_probes.second)));
    //    pr_sum = std::accumulate(&prism_probes.first[0], &*(prism_probes.first.end()-1), (PRISMATIC_FLOAT_PRECISION)0.0, [](std::complex<PRISMATIC_FLOAT_PRECISION> a){return abs(a);});

    //    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> debug = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{multislice_probes.first.get_dimj(), multislice_probes.first.get_dimi()}});
    //    for (auto j = 0; j < multislice_probes.first.get_dimj(); ++j){
    //        for (auto i = 0; i < multislice_probes.first.get_dimi(); ++i){
    //        debug.at(j,i) = std::abs(multislice_probes.first.at(j,i));
    //        }
    //    }
    //    debug.toMRC_f("/mnt/spareA/clion/PRISM/build/db.mrc");
    //    for (auto j = 0; j < multislice_probes.second.get_dimj(); ++j){
    //        for (auto i = 0; i < multislice_probes.second.get_dimi(); ++i){
    //        debug.at(j,i) = std::abs(multislice_probes.second.at(j,i));
    //        }
    //    }
    //    debug.toMRC_f("/mnt/spareA/clion/PRISM/build/dbk.mrc");

    //    for (auto j = 0; j < prism_probes.first.get_dimj(); ++j){
    //        for (auto i = 0; i < prism_probes.first.get_dimi(); ++i){
    //        debug.at(j,i) = std::abs(prism_probes.first.at(j,i));
    //        }
    //    }
    //    debug.toMRC_f("/mnt/spareA/clion/PRISM/build/db_p.mrc");
    //    for (auto j = 0; j < prism_probes.second.get_dimj(); ++j){
    //        for (auto i = 0; i < prism_probes.second.get_dimi(); ++i){
    //        debug.at(j,i) = std::abs(prism_probes.second.at(j,i));
    //        }
    //    }
    //    debug.toMRC_f("/mnt/spareA/clion/PRISM/build/dbk_p.mrc");

    //    std::cout << "prism_probes.first.get_dimj() = " << prism_probes.first.get_dimj() <<std::endl;
    //    std::cout << "multislice_probes.first.get_dimj() = " << multislice_probes.first.get_dimj() <<std::endl;

    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> pr = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{prism_probes.first.get_dimj(), prism_probes.first.get_dimi()}});
    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> pk = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{prism_probes.second.get_dimj(), prism_probes.second.get_dimi()}});
    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> mr = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{multislice_probes.first.get_dimj(), multislice_probes.first.get_dimi()}});
    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> mk = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{multislice_probes.second.get_dimj(), multislice_probes.second.get_dimi()}});
    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> diffr = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{multislice_probes.first.get_dimj(), multislice_probes.first.get_dimi()}});
    Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> diffk = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>({{multislice_probes.second.get_dimj(), multislice_probes.second.get_dimi()}});

    //if (use_log_scale){
    //    for (auto i = 0; i < prism_probes.first.size(); ++i){
    //        pr[i] =  std::log(1e-5 + std::abs(prism_probes.first[i]));
    //    }
    //    for (auto i = 0; i < prism_probes.second.size(); ++i){
    //        pk[i] =  std::log(1e-5 + std::abs(prism_probes.second[i]));
    //    }
    //    for (auto i = 0; i < multislice_probes.first.size(); ++i){
    //        mr[i] =  std::log(1e-5 + std::abs(multislice_probes.first[i]));
    //    }
    //    for (auto i = 0; i < multislice_probes.second.size(); ++i){
    //        mk[i] =  std::log(1e-5 + std::abs(multislice_probes.second[i]));
    //    }
    //    for (auto i = 0; i < prism_probes.second.size(); ++i){
    //        diffr[i] =  log(1e-5 + std::abs(pr[i] - mr[i]));
    //        diffk[i] =  log(1e-5 + std::abs(pk[i] - mk[i]));
    //    }
    //} else{
    for (auto i = 0; i < prism_probes.first.size(); ++i)
    {
        pr[i] = std::abs(prism_probes.first[i]);
    }
    for (auto i = 0; i < prism_probes.second.size(); ++i)
    {
        pk[i] = std::abs(prism_probes.second[i]);
    }
    for (auto i = 0; i < multislice_probes.first.size(); ++i)
    {
        mr[i] = std::abs(multislice_probes.first[i]);
    }
    for (auto i = 0; i < multislice_probes.second.size(); ++i)
    {
        mk[i] = std::abs(multislice_probes.second[i]);
    }
    for (auto i = 0; i < prism_probes.second.size(); ++i)
    {
        diffr[i] = (std::abs(pr[i] - mr[i]));
        diffk[i] = (std::abs(pk[i] - mk[i]));
    }
    //}

    emit signalProbeR_PRISM((pr));
    emit signalProbeK_PRISM(fftshift2(pk));
    emit signalProbeR_Multislice((mr));
    emit signalProbeK_Multislice(fftshift2(mk));
    emit signalProbe_diffR(diffr);
    emit signalProbe_diffK(fftshift2(diffk));
}

FullPRISMCalcThread::FullPRISMCalcThread(PRISMMainWindow *_parent, prism_progressbar *_progressbar) : PRISMThread(_parent, _progressbar){};

void FullPRISMCalcThread::run()
{
    //std::cout << "Full PRISM Calculation thread running" << std::endl;

    //bool error_reading = false;
    //QMutexLocker gatekeeper(&this->parent->dataLock);
    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);

    /*
    try {
        params = Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION>(meta,progressbar);
    }catch (...){
        std::cout <<"An error occurred while attempting to read from file " << meta.filenameAtoms << std::endl;
        error_reading = true;
    }
     */
    //gatekeeper.unlock();

    if (!Prismatic::testFilenameOutput(params.meta.filenameOutput.c_str()))
    {
        std::cout << "Aborting calculation, please choose an accessible output directory" << std::endl;
        return;
    }

    if (Prismatic::testFilenameOutput(params.meta.filenameOutput.c_str()) == 2)
    {
        emit overwriteWarning();
        if (this->parent->overwriteFile())
        {
            //params.outputFile.flush(H5F_SCOPE_GLOBAL);
            remove(params.meta.filenameOutput.c_str());
            this->thread()->sleep(1);
            this->parent->flipOverwrite(); //flip the check back
        }
        else
        {
            return;
        }
    }

    progressbar->signalDescriptionMessage("Initiating PRISM simulation");
    //emit signalTitle("PRISM: Frozen Phonon #1");
    //if (error_reading){
    //  emit signalErrorReadingAtomsDialog();
    // else {
    QMutexLocker calculationLocker(&this->parent->calculationLock);

    Prismatic::configure(meta);
    params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_TRUNC);
    Prismatic::setupOutputFile(params);
    params.fpFlag = 0;

    //  //  Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params = Prismatic::execute_plan(meta);
    if ((!this->parent->potentialIsReady()) || !(params.meta == *(this->parent->getMetadata())))
    {
        this->parent->resetCalculation(); // any time we are computing the potential we are effectively starting over the whole calculation, so make sure all flags are reset
        Prismatic::PRISM01_calcPotential(params);

        //Output potential if box is check
        /*
        if (this->parent->saveProjectedPotential){
          std::string outfile = Prismatic::remove_extension(params.meta.filenameOutput.c_str());
          outfile += ("_potential.mrc");
          std::cout<<"Outputting potential with filename "<<outfile<<std::endl;
          params.pot.toMRC_f(outfile.c_str());
        }
        */

        std::cout << "Potential Calculated" << std::endl;
        {
            QMutexLocker gatekeeper(&this->parent->dataLock);
            this->parent->pars = params;
        }
    }
    else
    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        params = this->parent->pars;
        params.progressbar = progressbar;
        std::cout << "Potential already calculated. Using existing result." << std::endl;
    }

    this->parent->potentialReceived(params.pot);
    emit potentialCalculated();

    Prismatic::PRISM02_calcSMatrix(params);
    /*
    if ((!this->parent->SMatrixIsReady())  || !(params.meta == *(this->parent->getMetadata()))){
            {
            //        QMutexLocker gatekeeper(&this->parent->dataLock);

            //        // perform copy
            //        this->parent->pars = params;

            //        // indicate that the potential is ready
            //        this->parent->ScompactReady = true;
            }
    } else {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        params = this->parent->pars;
        params.progressbar = progressbar;
        std::cout << "S-Matrix already calculated. Using existing result." << std::endl;
    }
    */
    //    emit ScompactCalculated();

    Prismatic::PRISM03_calcOutput(params);
    params.outputFile.close();

    if (params.meta.numFP > 1)
    {
        // run the rest of the frozen phonons
        Prismatic::Array4D<PRISMATIC_FLOAT_PRECISION> net_output(params.output);
        Prismatic::Array4D<PRISMATIC_FLOAT_PRECISION> DPC_CoM_output;
        if (params.meta.saveDPC_CoM)
            DPC_CoM_output = params.DPC_CoM;

        for (auto fp_num = 1; fp_num < params.meta.numFP; ++fp_num)
        {
            params.meta.randomSeed = rand() % 100000;
            ++meta.fpNum;
            Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
            emit signalTitle("PRISM: Frozen Phonon #" + QString::number(1 + fp_num));
            progressbar->resetOutputs();

            params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_RDWR);
            params.fpFlag = fp_num;

            Prismatic::PRISM01_calcPotential(params);
            this->parent->potentialReceived(params.pot);
            emit potentialCalculated();
            Prismatic::PRISM02_calcSMatrix(params);
            Prismatic::PRISM03_calcOutput(params);
            net_output += params.output;
            if (meta.saveDPC_CoM)
                DPC_CoM_output += params.DPC_CoM;
            params.outputFile.close();
        }
        // divide to take average
        for (auto &i : net_output)
            i /= params.meta.numFP;
        params.output = net_output;

        if (params.meta.saveDPC_CoM)
        {
            for (auto &j : DPC_CoM_output)
                j /= params.meta.numFP; //since squared intensities are used to calculate DPC_CoM, this is incoherent averaging
            params.DPC_CoM = DPC_CoM_output;
        }
    }

    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        this->parent->pars = params;
        gatekeeper.unlock();
    }

    {
        QMutexLocker gatekeeper(&this->parent->outputLock);
        this->parent->detectorAngles = params.detectorAngles;
        for (auto &a : this->parent->detectorAngles)
            a *= 1000; // convert to mrads
        this->parent->pixelSize = params.pixelSize;

        //        this->parent->outputArrayExists = true;

        //        params.output.toMRC_f(params.meta.filenameOutput.c_str());
        gatekeeper.unlock();
    }

    params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_RDWR);

    if (params.meta.save3DOutput)
    {
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setupVDOutput(params, params.output.get_diml(), dummy);
        Prismatic::Array3D<PRISMATIC_FLOAT_PRECISION> output_image = Prismatic::zeros_ND<3, PRISMATIC_FLOAT_PRECISION>({{params.output.get_dimj(), params.output.get_dimk(), params.output.get_dimi()}});

        std::stringstream nameString;
        nameString << "4DSTEM_simulation/data/realslices/virtual_detector_depth" << Prismatic::getDigitString(0);
        H5::Group dataGroup = params.outputFile.openGroup(nameString.str());

        std::string dataSetName = "realslice";
        H5::DataSet VD_data = dataGroup.openDataSet(dataSetName);
        hsize_t mdims[3] = {params.xp.size(), params.yp.size(), params.Ndet};

        for (auto b = 0; b < params.Ndet; b++)
        {
            for (auto y = 0; y < params.output.get_dimk(); ++y)
            {
                for (auto x = 0; x < params.output.get_dimj(); ++x)
                {
                    output_image.at(x, y, b) = params.output.at(0, y, x, b);
                }
            }
        }

        Prismatic::writeDatacube3D(VD_data, &output_image[0], mdims);
        VD_data.close();
        dataGroup.close();
    }

    if (params.meta.save2DOutput)
    {
        size_t lower = std::max((size_t)0, (size_t)(params.meta.integrationAngleMin / params.meta.detectorAngleStep));
        size_t upper = std::min((size_t)params.detectorAngles.size(), (size_t)(params.meta.integrationAngleMax / params.meta.detectorAngleStep));
        Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> prism_image;
        prism_image = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>(
            {{params.output.get_dimj(), params.output.get_dimk()}});
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setup2DOutput(params, params.output.get_diml(), dummy);

        for (auto y = 0; y < params.output.get_dimk(); ++y)
        {
            for (auto x = 0; x < params.output.get_dimj(); ++x)
            {
                for (auto b = lower; b < upper; ++b)
                {
                    prism_image.at(x, y) += params.output.at(0, y, x, b);
                }
            }
        }
        std::stringstream nameString;
        nameString << "4DSTEM_simulation/data/realslices/annular_detector_depth" << Prismatic::getDigitString(0);
        H5::Group dataGroup = params.outputFile.openGroup(nameString.str());
        H5::DataSet AD_data = dataGroup.openDataSet("realslice");
        hsize_t mdims[2] = {params.xp.size(), params.yp.size()};

        Prismatic::writeRealSlice(AD_data, &prism_image[0], mdims);
        AD_data.close();
        dataGroup.close();

        //std::string image_filename = params.meta.outputFolder + std::string("prism_2Doutput_") + params.meta.filenameOutput;
        //prism_image.toMRC_f(image_filename.c_str());
    }

    if (params.meta.saveDPC_CoM)
    {
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setupDPCOutput(params, params.output.get_diml(), dummy);

        //create dummy array to pass to
        Prismatic::Array3D<PRISMATIC_FLOAT_PRECISION> DPC_slice;

        std::stringstream nameString;
        nameString << "4DSTEM_simulation/data/realslices/DPC_CoM_depth" << Prismatic::getDigitString(0);
        H5::Group dataGroup = params.outputFile.openGroup(nameString.str());
        hsize_t mdims[3] = {params.xp.size(), params.yp.size(), 2};
        std::string dataSetName = "realslice";
        H5::DataSet DPC_data = dataGroup.openDataSet(dataSetName);

        DPC_slice = Prismatic::zeros_ND<3, PRISMATIC_FLOAT_PRECISION>({{params.DPC_CoM.get_dimj(), params.DPC_CoM.get_dimk(), 2}});
        for (auto b = 0; b < params.DPC_CoM.get_dimi(); ++b)
        {
            for (auto y = 0; y < params.DPC_CoM.get_dimk(); ++y)
            {
                for (auto x = 0; x < params.DPC_CoM.get_dimj(); ++x)
                {
                    DPC_slice.at(x, y, b) = params.DPC_CoM.at(0, y, x, b);
                }
            }
        }

        Prismatic::writeDatacube3D(DPC_data, &DPC_slice[0], mdims);
        DPC_data.close();
        dataGroup.close();
    }

    PRISMATIC_FLOAT_PRECISION dummy = 1.0;
    Prismatic::writeMetadata(params, dummy);
    params.outputFile.close();

    this->parent->outputReceived(params.output);
    emit outputCalculated();
    std::cout << "PRISM calculation complete" << std::endl;

    calculationLocker.unlock();
}

FullMultisliceCalcThread::FullMultisliceCalcThread(PRISMMainWindow *_parent, prism_progressbar *_progressbar) : PRISMThread(_parent, _progressbar){};

void FullMultisliceCalcThread::run()
{

    Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
    if (!Prismatic::testFilenameOutput(params.meta.filenameOutput.c_str()))
    {
        std::cout << "Aborting calculation, please choose an accessible output directory" << std::endl;
        return;
    }

    if (Prismatic::testFilenameOutput(params.meta.filenameOutput.c_str()) == 2)
    {
        emit overwriteWarning();
        if (this->parent->overwriteFile())
        {
            //params.outputFile.flush(H5F_SCOPE_GLOBAL);
            remove(params.meta.filenameOutput.c_str());
            this->thread()->sleep(1);
            this->parent->flipOverwrite(); //flip the check back
        }
        else
        {
            return;
        }
    }

    progressbar->signalDescriptionMessage("Initiating Multislice simulation");

    std::cout << "Also do CPU work: " << params.meta.alsoDoCPUWork << std::endl;
    QMutexLocker calculationLocker(&this->parent->calculationLock);
    Prismatic::configure(meta);

    params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_TRUNC);
    Prismatic::setupOutputFile(params);
    params.fpFlag = 0;

    if ((!this->parent->potentialIsReady()) || !(params.meta == *(this->parent->getMetadata())))
    {
        this->parent->resetCalculation(); // any time we are computing the potential we are effectively starting over the whole calculation, so make sure all flags are reset
        Prismatic::PRISM01_calcPotential(params);

        std::cout << "Potential Calculated" << std::endl;
        /* 
            if (this->parent->saveProjectedPotential){
              std::string outfile = Prismatic::remove_extension(params.meta.filenameOutput.c_str());
              outfile += ("_potential.mrc");
              std::cout<<"Outputting potential with filename "<<outfile<<std::endl;
              params.pot.toMRC_f(outfile.c_str());
            }
            */
        {
            QMutexLocker gatekeeper(&this->parent->dataLock);
            this->parent->pars = params;
            //        this->parent->potential = params.pot;

            //              this->parent->potentialArrayExists = true;
        }
    }
    else
    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        params = this->parent->pars;
        params.progressbar = progressbar;
        std::cout << "Potential already calculated. Using existing result." << std::endl;
    }

    this->parent->potentialReceived(params.pot);
    emit potentialCalculated();
    std::cout << "Also do CPU work: " << params.meta.alsoDoCPUWork << std::endl;

    params.scale = 1.0;
    //Calls Multislice_calcOutput for first frozen phonon pass
    Prismatic::Multislice_calcOutput(params);
    params.outputFile.close();

    if (params.meta.numFP > 1)
    {
        // run the rest of the frozen phonons
        Prismatic::Array4D<PRISMATIC_FLOAT_PRECISION> net_output(params.output);
        Prismatic::Array4D<PRISMATIC_FLOAT_PRECISION> DPC_CoM_output;
        if (params.meta.saveDPC_CoM)
            DPC_CoM_output = params.DPC_CoM;
        for (auto fp_num = 1; fp_num < params.meta.numFP; ++fp_num)
        {
            params.meta.randomSeed = rand() % 100000;
            ++meta.fpNum;
            Prismatic::Parameters<PRISMATIC_FLOAT_PRECISION> params(meta, progressbar);
            emit signalTitle("PRISM: Frozen Phonon #" + QString::number(1 + fp_num));
            progressbar->resetOutputs();

            params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_RDWR);
            params.fpFlag = fp_num;
            params.scale = 1.0;

            Prismatic::PRISM01_calcPotential(params);
            this->parent->potentialReceived(params.pot);
            emit potentialCalculated();
            Prismatic::Multislice_calcOutput(params);

            net_output += params.output;
            if (meta.saveDPC_CoM)
                DPC_CoM_output += params.DPC_CoM;
            params.outputFile.close();
        }
        // divide to take average
        for (auto &i : net_output)
            i /= params.meta.numFP;
        params.output = net_output;

        if (params.meta.saveDPC_CoM)
        {
            for (auto &j : DPC_CoM_output)
                j /= params.meta.numFP; //since squared intensities are used to calculate DPC_CoM, this is incoherent averaging
            params.DPC_CoM = DPC_CoM_output;
        }
    }

    {
        QMutexLocker gatekeeper(&this->parent->dataLock);
        this->parent->pars = params;
        gatekeeper.unlock();
    }
    {
        QMutexLocker gatekeeper(&this->parent->outputLock);
        this->parent->detectorAngles = params.detectorAngles;
        for (auto &a : this->parent->detectorAngles)
            a *= 1000; // convert to mrads
        this->parent->pixelSize = params.pixelSize;

        //        this->parent->outputArrayExists = true;
        //        Prismatic::Array3D<PRISMATIC_FLOAT_PRECISION> reshaped_output = Prismatic::zeros_ND<3, PRISMATIC_FLOAT_PRECISION>(
        //        {{params.output.get_diml(), params.output.get_dimk(), params.output.get_dimj()}});
        //        auto ptr = reshaped_output.begin();
        //        for (auto &i:params.output)*ptr++=i;
        //        reshaped_output.toMRC_f(params.meta.filenameOutput.c_str());
        //        params.output.toMRC_f(params.meta.filenameOutput.c_str());
        gatekeeper.unlock();
    }

    params.outputFile = H5::H5File(params.meta.filenameOutput.c_str(), H5F_ACC_RDWR);

    if (params.meta.save3DOutput)
    {
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setupVDOutput(params, params.output.get_diml(), dummy);

        //create dummy array to pass to
        Prismatic::Array3D<PRISMATIC_FLOAT_PRECISION> slice_image;
        slice_image = Prismatic::zeros_ND<3, PRISMATIC_FLOAT_PRECISION>({{params.output.get_dimj(), params.output.get_dimk(), params.output.get_dimi()}});

        for (auto j = 0; j < params.output.get_diml(); j++)
        {
            std::stringstream nameString;
            nameString << "4DSTEM_simulation/data/realslices/virtual_detector_depth" << Prismatic::getDigitString(j);
            H5::Group dataGroup = params.outputFile.openGroup(nameString.str());
            hsize_t mdims[3] = {params.xp.size(), params.yp.size(), params.Ndet};

            std::string dataSetName = "realslice";
            H5::DataSet VD_data = dataGroup.openDataSet(dataSetName);
            for (auto b = 0; b < params.output.get_dimi(); ++b)
            {
                for (auto y = 0; y < params.output.get_dimk(); ++y)
                {
                    for (auto x = 0; x < params.output.get_dimj(); ++x)
                    {
                        slice_image.at(x, y, b) = params.output.at(j, y, x, b);
                    }
                }
            }
            Prismatic::writeDatacube3D(VD_data, &slice_image[0], mdims);
            VD_data.close();
            dataGroup.close();
        }
    }

    if (params.meta.save2DOutput)
    {
        size_t lower = std::max((size_t)0, (size_t)(params.meta.integrationAngleMin / params.meta.detectorAngleStep));
        size_t upper = std::min(params.detectorAngles.size(), (size_t)(params.meta.integrationAngleMax / params.meta.detectorAngleStep));
        Prismatic::Array2D<PRISMATIC_FLOAT_PRECISION> prism_image;
        //std::string image_filename = std::string("multislice_2Doutput")+params.meta.filenameOutput;
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setup2DOutput(params, params.output.get_diml(), dummy);

        for (auto j = 0; j < params.output.get_diml(); j++)
        {
            //need to initiliaze output image at each slice to prevent overflow of value
            prism_image = Prismatic::zeros_ND<2, PRISMATIC_FLOAT_PRECISION>(
                {{params.output.get_dimj(), params.output.get_dimk()}});

            for (auto y = 0; y < params.output.get_dimk(); ++y)
            {
                for (auto x = 0; x < params.output.get_dimj(); ++x)
                {
                    for (auto b = lower; b < upper; ++b)
                    {
                        prism_image.at(x, y) += params.output.at(j, y, x, b);
                    }
                }
            }
            //if (params.meta.numSlices != 0){
            //	image_filename = params.meta.outputFolder +  (std::string("multislice_2Doutput_slice") + std::to_string(j) + std::string("_")) + params.meta.filenameOutput;
            //}
            //prism_image.toMRC_f(image_filename.c_str());
            std::stringstream nameString;
            nameString << "4DSTEM_simulation/data/realslices/annular_detector_depth" << Prismatic::getDigitString(j);
            H5::Group dataGroup = params.outputFile.openGroup(nameString.str());
            H5::DataSet AD_data = dataGroup.openDataSet("realslice");
            hsize_t mdims[2] = {params.xp.size(), params.yp.size()};

            Prismatic::writeRealSlice(AD_data, &prism_image[0], mdims);
            AD_data.close();
            dataGroup.close();
        }
    }

    if (params.meta.saveDPC_CoM)
    {
        PRISMATIC_FLOAT_PRECISION dummy = 1.0;
        Prismatic::setupDPCOutput(params, params.output.get_diml(), dummy);

        //create dummy array to pass to
        Prismatic::Array3D<PRISMATIC_FLOAT_PRECISION> DPC_slice;
        DPC_slice = Prismatic::zeros_ND<3, PRISMATIC_FLOAT_PRECISION>({{params.DPC_CoM.get_dimj(), params.DPC_CoM.get_dimk(), 2}});

        for (auto j = 0; j < params.output.get_diml(); j++)
        {
            std::stringstream nameString;
            nameString << "4DSTEM_simulation/data/realslices/DPC_CoM_depth" << Prismatic::getDigitString(j);
            H5::Group dataGroup = params.outputFile.openGroup(nameString.str());
            hsize_t mdims[3] = {params.xp.size(), params.yp.size(), 2};
            std::string dataSetName = "realslice";
            H5::DataSet DPC_data = dataGroup.openDataSet(dataSetName);

            for (auto b = 0; b < params.DPC_CoM.get_dimi(); ++b)
            {
                for (auto y = 0; y < params.DPC_CoM.get_dimk(); ++y)
                {
                    for (auto x = 0; x < params.DPC_CoM.get_dimj(); ++x)
                    {
                        DPC_slice.at(x, y, b) = params.DPC_CoM.at(j, y, x, b);
                    }
                }
            }

            Prismatic::writeDatacube3D(DPC_data, &DPC_slice[0], mdims);
            DPC_data.close();
            dataGroup.close();
        }
    }

    PRISMATIC_FLOAT_PRECISION dummy = 1.0;
    Prismatic::writeMetadata(params, dummy);
    params.outputFile.close();

    this->parent->outputReceived(params.output);
    emit outputCalculated();
    std::cout << "Multislice calculation complete" << std::endl;

    calculationLocker.unlock();
}

PRISMThread::~PRISMThread() {}
PotentialThread::~PotentialThread() {}
//SMatrixThread::~SMatrixThread(){}
ProbeThread::~ProbeThread() {}
FullPRISMCalcThread::~FullPRISMCalcThread() {}
FullMultisliceCalcThread::~FullMultisliceCalcThread() {}
