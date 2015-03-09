/*
 * ParserGiD.h
 *
 *  Created on: Sep 23, 2013
 *      Author: luis
 */

#ifndef PARSERGID_H_
#define PARSERGID_H_

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include "Parser.h"
#include "../SmbData.h"
#include "ProblemSize.h"

#define LABEL_ENDING ':'

class ParserGiD : public Parser, public ProjectFile {
    friend class SmbData;
public:
    ParserGiD();
    ParserGiD(
            const string& fn);
    ParserGiD(
            const string& fn,
            const string& pTPath);
    virtual
    ~ParserGiD();
    virtual SmbData*
    read();
    virtual const ProblemSize*
    getProblemSize() const;
    virtual void
    printInfo() const;
protected:
    string problemTypePath_;
    ProblemSize pSize_;
    virtual SolverParameters* readSolverParameters();
    virtual LayerGroup<>* readLayers();
    virtual EMSourceGroup<>* readEMSources();
    virtual OutRqGroup<>* readOutputRequests();
    virtual MesherParameters* readMesherParameters();
    virtual PhysicalModelGroup<>* readMaterials();
    virtual Mesh* readMesh();
    ProblemSize readProblemSize();
    PMVolumeDispersive* readDispersiveMatFile(
            const unsigned int id_,
            const string& name) const;
    PMSurfaceSIBC* readIsotropicSurfMatFile(
            const int id,
            const string& name) const;
    PMSurfaceMultilayer*  readMultilayerSurf(
            const int id,
            const string& name,
            const string& layersString) const;
    CoordinateGroup<>* readCoordinates();
    ElementsGroup readElements(const CoordinateGroup<>&);
    vector<Hex8> readHex8Elements(const CoordinateGroup<>& v);
    vector<Tet10> readTet10Elements(const CoordinateGroup<>& v);
    vector<Tet4> readTet4Elements(const CoordinateGroup<>& v);
    vector<Tri6> readTri6Elements(const CoordinateGroup<>& v);
    vector<Tri3> readTri3Elements(const CoordinateGroup<>& v);
    vector<Lin2> readLin2Elements(const CoordinateGroup<>& v);
    Grid3* readCartesianGrid();
    void readOutRqInstances(OutRqGroup<>* res);
    void getNextLabelAndValue(string& label, string& value);
private:
    typedef enum {
        outRqOnPoint,
        outRqOnLine,
        outRqOnSurface,
        outRqOnVolume,
        farField,
        undefined
    } GiDOutputType;
    typedef enum {
        sibc,
        multilayer,
        undefinedSIBC
    } SIBCType;
    const CoordinateGroup<>* cG_;
    PlaneWave* readPlaneWave();
    Dipole* readDipole();
    Waveport* readWaveport();
    Generator* readGenerator();
    SourceOnLine* readSourceOnLine();
    PhysicalModel* readPhysicalModel(const uint id);
    Magnitude* readMagnitude(const string type);
    pair<CVecD3, CVecD3> strToBound(const string& str) const;
    Generator::Type strToGeneratorType(string label) const;
    Generator::Hardness strToGeneratorHardness(string str) const;
    SourceOnLine::Type strToNodalType(string label) const;
    SourceOnLine::Hardness strToNodalHardness(string label) const;
    SolverParameters::boundType strToBoundType(string label) const;
    PhysicalModelGroup<>::Type strToMaterialType(string label) const;
    PMMultiport::Type strToMultiportType(string label) const;
    Element::Type strToElementType(string label) const;
    OutRq::Type strToOutputType(string label) const;
    SIBCType strToSIBCType(string str) const;
    GiDOutputType strToGidOutputType(string label) const;
    Domain strToDomain(string line) const;
    MesherParameters::Mesher strToMesher(string) const;
    MesherParameters::Mode strToMesherMode(string) const;
    SolverParameters::Solver strToSolver(string) const;
    void init(const string& pTPath);
    string readVersion();
    bool checkVersionCompatibility(const string version) const;
};

#endif /* PARSERGID_H_ */
