// OpenSEMBA
// Copyright (C) 2015 Salvador Gonzalez Garcia        (salva@ugr.es)
//                    Luis Manuel Diaz Angulo         (lmdiazangulo@semba.guru)
//                    Miguel David Ruiz-Cabello Nuñez (miguel@semba.guru)
//                    Daniel Mateos Romero            (damarro@semba.guru)
//
// This file is part of OpenSEMBA.
//
// OpenSEMBA is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// OpenSEMBA is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OpenSEMBA. If not, see <http://www.gnu.org/licenses/>.

#include "../json/Parser.h"

#include "math/function/Gaussian.h"
#include "geometry/element/Line2.h"
#include "geometry/element/Triangle3.h"
#include "geometry/element/Triangle6.h"
#include "geometry/element/Quadrilateral4.h"
#include "geometry/element/Tetrahedron4.h"
#include "geometry/element/Tetrahedron10.h"
#include "geometry/element/Hexahedron8.h"
#include "physicalModel/bound/Bound.h"
#include "physicalModel/predefined/PEC.h"
#include "physicalModel/predefined/PMC.h"
#include "physicalModel/predefined/SMA.h"
#include "physicalModel/multiport/Dispersive.h"
#include "physicalModel/multiport/Predefined.h"
#include "physicalModel/multiport/RLC.h"
#include "physicalModel/volume/AnisotropicCrystal.h"
#include "physicalModel/volume/AnisotropicFerrite.h"
#include "physicalModel/volume/Classic.h"
#include "physicalModel/volume/PML.h"
#include "physicalModel/wire/Wire.h"
#include "physicalModel/gap/Gap.h"
#include "source/port/WaveguideRectangular.h"
#include "source/port/TEMCoaxial.h"
#include "outputRequest/BulkCurrent.h"
#include "outputRequest/FarField.h"

namespace SEMBA {
namespace Parser {
namespace JSON {

Data Parser::read(std::istream& stream) const {

    json j;
    try {
        stream >> j;
    }
    catch( const std::exception & ex ) {
        std::cerr << ex.what() << std::endl;
    }

    Util::ProgressBar progress;
    progress.init("Parser GiD-JSON", 7, 0);

    std::string version = j.at("_version").get<std::string>();
    if (!checkVersionCompatibility(version)) {
        throw std::logic_error(
                "File version " + version + " is not supported.");
    }
    progress.advance();

    Data res;

    res.solver = readSolver(j);
    progress.advance();

    res.physicalModels = readPhysicalModels(j);
    progress.advance();

    res.mesh = readGeometricMesh(*res.physicalModels, j);
    progress.advance();

    res.sources = readSources(*res.mesh, j);
    progress.advance();

    res.outputRequests = readOutputRequests(*res.mesh, j);
    progress.advance();

    postReadOperations(res);
    progress.advance();

    progress.end();

    return res;
}

Solver::Info* Parser::readSolver(const json& j) {
    if (j.find("solverOptions") == j.end()) {
        return nullptr;
    }
    json solverOptions = j.at("solverOptions").get<json>();
    Solver::Settings opts = readSolverSettings(solverOptions);
    return new Solver::Info(
            solverOptions.at("solverName").get<std::string>(),
            std::move(opts));
}

Solver::Settings Parser::readSolverSettings(const json& j) {
    Solver::Settings opts;
    opts.setObject();
    for (json::const_iterator it = j.begin(); it != j.end(); ++it) {
        if (it->is_object()) {
            Solver::Settings aux = readSolverSettings(*it);
            opts.addMember(it.key(), std::move(aux));
        } else {
            Solver::Settings aux;
            std::stringstream value;
            value << *it;
            aux.setString(value.str());
            opts.addMember(it.key(), std::move(aux));
        }
    }
    return opts;
}

Geometry::Mesh::Geometric* Parser::readGeometricMesh(
        const PhysicalModel::Group<>& physicalModels, const json& j) {
    Geometry::Grid3 grid = readGrids(j);
    Geometry::Layer::Group<> layers = readLayers(j);
    Geometry::Coordinate::Group<Geometry::CoordR3> coords = readCoordinates(j);
    Geometry::Element::Group<Geometry::ElemR> elements =
            readElements(physicalModels, layers, coords, j);
    return new Geometry::Mesh::Geometric(grid, coords, elements, layers);
}

Source::Group<>* Parser::readSources(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    if (j.find("sources") == j.end()) {
        throw std::logic_error("Sources label was not found.");
    }

    Source::Group<>* res = new Source::Group<>();
    const json& sources = j.at("sources").get<json>();
    for (json::const_iterator it = sources.begin(); it != sources.end(); ++it) {
        std::string sourceType = it->at("sourceType").get<std::string>();

        if (sourceType.compare("planewave") == 0) {
            res->add(readPlanewave(mesh, *it));
        } else if (sourceType.compare("generator") == 0) {
            res->add(readGenerator(*it));
        } else if (sourceType.compare("sourceOnLine") == 0) {
            res->add(readSourceOnLine(*it));
        } else if (sourceType.compare("waveguidePort") == 0) {
            res->add(readPortWaveguide(*it));
        } else if (sourceType.compare("temPort") == 0) {
            res->add(readPortTEM(*it));
        } else {
            throw std::logic_error("Unrecognized source type: " + sourceType);
        }
    }
    return res;
}

PhysicalModel::Group<>* Parser::readPhysicalModels(const json& j){
    if (j.find("materials") == j.end()) {
        return nullptr;
    }
    PhysicalModel::Group<>* res = new PhysicalModel::Group<>();
    json materials = j.at("materials");
    for (json::const_iterator it = materials.begin();
            it != materials.end(); ++it) {
        res->add(readPhysicalModel( *it ));
    }
    return res;
}

PhysicalModel::PhysicalModel* Parser::readPhysicalModel(const json& j) {
    PhysicalModel::PhysicalModel::Type type =
                strToMaterialType( j.at("materialType").get<std::string>() );
    MatId id = MatId(j.at("materialId").get<int>());
    std::string name = j.at("name").get<std::string>();

    switch (type) {
    case PhysicalModel::PhysicalModel::PEC:
        return new PhysicalModel::Predefined::PEC (id, name);

    case PhysicalModel::PhysicalModel::PMC:
        return new PhysicalModel::Predefined::PMC(id, name);

    case PhysicalModel::PhysicalModel::SMA:
        return new PhysicalModel::Predefined::SMA(id, name);

    case PhysicalModel::PhysicalModel::PML:
        if (j.at("automaticOrientation").get<bool>()) {
            return new PhysicalModel::Volume::PML(id, name, nullptr);
        } else {
            Math::Axis::Local* localAxes;
            localAxes = new Math::Axis::Local(
                    strToLocalAxes(j.at("localAxes").get<std::string>()));
            return new PhysicalModel::Volume::PML(id, name, localAxes);
        }

    case PhysicalModel::PhysicalModel::classic:
        return new PhysicalModel::Volume::Classic(id, name,
                j.at("permittivity").get<double>(),
                j.at("permeability").get<double>(),
                j.at("electricConductivity").get<double>(),
                j.at("magneticConductivity").get<double>());

    case PhysicalModel::PhysicalModel::elecDispersive:
        return new PhysicalModel::Volume::Dispersive(id, name,
                j.at("filename").get<std::string>());

    case PhysicalModel::PhysicalModel::wire:
    {
        std::string wireType = j.at("wireType").get<std::string>();
        if (wireType.compare("Dispersive") == 0) {
            return new PhysicalModel::Wire::Wire(id, name,
                    j.at("radius").get<double>(),
                    j.at("filename").get<std::string>());
        } else if(wireType.compare("SeriesParallel") == 0) {
            return new PhysicalModel::Wire::Wire(id, name,
                    j.at("radius").get<double>(),
                    j.at("resistance").get<double>(),
                    j.at("inductance").get<double>(),
                    j.at("capacitance").get<double>(),
                    j.at("parallelResistance").get<double>(),
                    j.at("parallelInductance").get<double>(),
                    j.at("parallelCapacitance").get<double>());
        } else if(wireType.compare("Standard") == 0) {
            return new PhysicalModel::Wire::Wire(id, name,
                    j.at("resistance"),
                    j.at("inductance"));
        } else {
            throw std::logic_error("Unrecognized wire type" + wireType);
        }
    }

    case PhysicalModel::PhysicalModel::anisotropic:
    {
        std::string str = j.at("anisotropicModel").get<std::string>();
        if (str.compare("Crystal")==0) {
            return new PhysicalModel::Volume::AnisotropicCrystal(id, name,
                    strToLocalAxes(j.at("localAxes").get<std::string>()),
                    strToCVecR3(
                            j.at("relativePermittiviy").get<std::string>()),
                    j.at("crystalRelativePermeability").get<double>());
        } else if (str.compare("Ferrite")==0) {
            return new PhysicalModel::Volume::AnisotropicFerrite(id, name,
                    strToLocalAxes(j.at("localAxes").get<std::string>()),
                    j.at("kappa").get<double>(),
                    j.at("ferriteRelativePermeability").get<double>(),
                    j.at("ferriteRelativePermittivity").get<double>());
        } else {
            throw std::logic_error("Unrecognized Anisotropic Model: " + str);
        }
    }

    case PhysicalModel::PhysicalModel::isotropicsibc:
    {
        std::string sibcType = j.at("surfaceType").get<std::string>();
        if (sibcType.compare("File")==0) {
            return new PhysicalModel::Surface::SIBC(id, name,
                    j.at("filename").get<std::string>() );
        } else if (sibcType.compare("Layers")==0) {
            return readMultilayerSurface(id, name,
                    j.at("layers").get<json>());
        } else {
            throw std::logic_error("Unrecognized SIBC type: " + sibcType);
        }
    }

    case PhysicalModel::PhysicalModel::gap:
        return new PhysicalModel::Gap::Gap(id, name,
                j.at("width").get<double>());

    case PhysicalModel::PhysicalModel::multiport:
    {
        PhysicalModel::Multiport::Multiport::Type mpType =
                strToMultiportType(j.at("connectorType").get<std::string>());
        if (mpType == PhysicalModel::Multiport::Multiport::shortCircuit) {
            return new PhysicalModel::Multiport::Predefined(id, name, mpType);
        } else if (mpType == PhysicalModel::Multiport::Multiport::dispersive) {
            return new PhysicalModel::Multiport::Dispersive(id, name,
                    j.at("filename").get<std::string>());
        } else {
            return new PhysicalModel::Multiport::RLC(id, name, mpType,
                    j.at("resistance").get<double>(),
                    j.at("inductance").get<double>(),
                    j.at("capacitance").get<double>());
        }
    }

    default:
        throw std::logic_error("Material type not recognized for: " + name);
    }
}

OutputRequest::Group<>* Parser::readOutputRequests(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    if (j.find("outputRequests") == j.end()) {
        throw std::logic_error("Output requests label was not found.");
    }

    OutputRequest::Group<>* res = new OutputRequest::Group<>();
    const json& outs = j.at("outputRequests").get<json>();
    for (json::const_iterator it = outs.begin(); it != outs.end(); ++it) {
        res->add(readOutputRequest(*it));
    }
    return res;
}

Geometry::Element::Group<> Parser::boxToElemGroup(
        Geometry::Mesh::Geometric& mesh,
        const std::string& line) {
    Geometry::BoxR3 box = strToBox(line);
    if (box.isVolume()) {
        Geometry::HexR8* hex =
            new Geometry::HexR8(mesh, Geometry::ElemId(0), box);
        mesh.elems().addId(hex);
        Geometry::Element::Group<> elems(hex);
        return elems;
    } else if (box.isSurface()) {
        Geometry::QuaR4* qua =
            new Geometry::QuaR4(mesh, Geometry::ElemId(0), box);
        mesh.elems().addId(qua);
        Geometry::Element::Group<> elems(qua);
        return elems;
    } else {
        throw std::logic_error(
                "Box to Elem Group only works for volumes and surfaces");
    }
}

OutputRequest::Base* Parser::readOutputRequest(
            Geometry::Mesh::Geometric& mesh, const json& j) {

    std::string gidOutputTypeStr = j.at("gidOutputType").get<std::string>();
    const OutputType gidOutputType = strToGiDOutputType(gidOutputTypeStr);

    std::string name = j.at("name").get<std::string>();
    OutputRequest::OutputRequest::Type type =
            strToOutputType(j.at("type").get<std::string>());
    OutputRequest::Domain domain = readDomain(j.at("domain").get<json>());

    switch (gidOutputType) {
    case Parser::outRqOnPoint:
        return new OutputRequest::OutputRequest<Geometry::Nod>(
                domain, type, name,
                readAsNodes(mesh, j.at("elemIds").get<json>()));
    case Parser::outRqOnLine:
        return new OutRqLine(domain, type, name,
                readAsLines(mesh, j.at("elemIds").get<json>()));
    case Parser::outRqOnSurface:
        return new OutRqSurface(domain, type, name,
                readAsSurfaces(mesh, j.at("elemIds").get<json>()));
    case Parser::outRqOnLayer:
    {
        Geometry::Element::Group<> elems =
                boxToElemGroup(mesh, j.at("box").get<std::string>());
        if (elems.sizeOf<Geometry::Vol>()) {
            return new OutputRequest::OutputRequest<Geometry::Vol>(
                    domain, type, name, elems.getOf<Geometry::Vol>());
        } else if (elems.sizeOf<Geometry::Surf>()) {
            return new OutputRequest::OutputRequest<Geometry::Surf>(
                    domain, type, name, elems.getOf<Geometry::Surf>());
        } else {
            throw std::logic_error(
                    "Layer for OutRqOnLayer must be volume or surface");
        }
    }
    case Parser::bulkCurrentOnSurface:
        return new OutputRequest::BulkCurrent(domain, name,
                readAsSurfaces(mesh, j.at("elemIds").get<json>()),
                strToCartesianAxis(j.at("direction").get<std::string>()),
                j.at("skip").get<int>());
    case Parser::bulkCurrentOnLayer:
        return new OutputRequest::BulkCurrent(domain, name,
                boxToElemGroup(mesh, j.at("box").get<std::string>()),
                strToCartesianAxis(j.at("direction").get<std::string>()),
                j.at("skip").get<int>());

    case Parser::farField:
    {
        static const Math::Real degToRad = 2.0 * Math::Constants::pi / 360.0;
        return new OutputRequest::FarField(domain, name,
                boxToElemGroup(mesh, j.at("box").get<std::string>()),
                j.at("initialTheta").get<double>() * degToRad,
                j.at("finalTheta").get<double>()   * degToRad,
                j.at("stepTheta").get<double>()    * degToRad,
                j.at("initialPhi").get<double>()   * degToRad,
                j.at("finalPhi").get<double>()     * degToRad,
                j.at("stepPhi").get<double>()      * degToRad);
    }
    default:
        throw std::logic_error(
                "Unrecognized GiD Output request type: " + gidOutputTypeStr);
    }
}

Math::Constants::CartesianAxis Parser::strToCartesianAxis(std::string str) {
    if (str.compare("x") == 0) {
        return Math::Constants::x;
    } else if (str.compare("y") == 0) {
        return Math::Constants::y;
    } else if (str.compare("z") == 0) {
        return Math::Constants::z;
    } else {
        throw std::logic_error("Unrecognized cartesian axis label: " + str);
    }
}

Geometry::Layer::Group<> Parser::readLayers(const json& j) {
    if (j.find("layers") == j.end()) {
        throw std::logic_error("layers object was not found.");
    }

    Geometry::Layer::Group<> res;
    const json layers = j.at("layers");
    for (json::const_iterator it = layers.begin(); it != layers.end(); ++it) {
        res.add(new Geometry::Layer::Layer(
                Geometry::Layer::Id(it->at("id").get<int>()),
                it->at("name").get<std::string>()));
    }
    return res;
}

Geometry::Coordinate::Group<Geometry::CoordR3> Parser::readCoordinates(
        const json& j) {

    if (j.find("coordinates") == j.end()) {
        throw std::logic_error("Coordinates label was not found.");
    }

    Geometry::Coordinate::Group<Geometry::CoordR3> res;
    const json& c = j.at("coordinates").get<json>();
    for (json::const_iterator it = c.begin(); it != c.end(); ++it) {
        Geometry::CoordId id;
        Math::CVecR3 pos;
        std::stringstream ss(it->get<std::string>());
        ss >> id >> pos(0) >> pos(1) >> pos(2);
        res.add(new Geometry::CoordR3(id, pos));
    }
    return res;
}

Geometry::Element::Group<Geometry::ElemR> Parser::readElements(
        const PhysicalModel::Group<>& mG,
        const Geometry::Layer::Group<>& lG,
        const Geometry::CoordR3Group& cG,
        const json& j) {

    if (j.find("elements") == j.end()) {
        throw std::logic_error("Elements label was not found.");
    }

    Geometry::Element::Group<Geometry::ElemR> res;
    const json& elems = j.at("elements").get<json>();

    {
        const json& e = elems.at("hexahedra").get<json>();
        for (json::const_iterator it = e.begin(); it != e.end(); ++it) {
            ParsedElementIds elemIds =
                    readElementIds(it->get<std::string>(), 8);
            ParsedElementPtrs elemPtrs =
                    convertElementIdsToPtrs(elemIds, mG, lG, cG);
            res.add(new Geometry::HexR8(elemIds.elemId,
                    elemPtrs.vPtr.data(), elemPtrs.layerPtr, elemPtrs.matPtr));
        }
    }

    {
        const json& e = elems.at("tetrahedra").get<json>();
        for (json::const_iterator it = e.begin(); it != e.end(); ++it) {
            ParsedElementIds elemIds =
                    readElementIds(it->get<std::string>(), 4);
            ParsedElementPtrs elemPtrs =
                    convertElementIdsToPtrs(elemIds, mG, lG, cG);
            res.add(new Geometry::Tet4(elemIds.elemId,
                    elemPtrs.vPtr.data(), elemPtrs.layerPtr, elemPtrs.matPtr));
        }
    }

    {
        const json& e = elems.at("quadrilateral").get<json>();
        for (json::const_iterator it = e.begin(); it != e.end(); ++it) {
            ParsedElementIds elemIds =
                    readElementIds(it->get<std::string>(), 4);
            ParsedElementPtrs elemPtrs =
                    convertElementIdsToPtrs(elemIds, mG, lG, cG);
            res.add(new Geometry::QuaR4(elemIds.elemId,
                    elemPtrs.vPtr.data(), elemPtrs.layerPtr, elemPtrs.matPtr));
        }
    }

    {
        const json& e = elems.at("triangle").get<json>();
        for (json::const_iterator it = e.begin(); it != e.end(); ++it) {
            ParsedElementIds elemIds =
                    readElementIds(it->get<std::string>(), 3);
            ParsedElementPtrs elemPtrs =
                    convertElementIdsToPtrs(elemIds, mG, lG, cG);
            res.add(new Geometry::Tri3(elemIds.elemId,
                    elemPtrs.vPtr.data(), elemPtrs.layerPtr, elemPtrs.matPtr));
        }
    }

    {
        const json& e = elems.at("line").get<json>();
        for (json::const_iterator it = e.begin(); it != e.end(); ++it) {
            ParsedElementIds elemIds =
                    readElementIds(it->get<std::string>(), 4);
            ParsedElementPtrs elemPtrs =
                    convertElementIdsToPtrs(elemIds, mG, lG, cG);
            res.add(new Geometry::LinR2(elemIds.elemId,
                    elemPtrs.vPtr.data(), elemPtrs.layerPtr, elemPtrs.matPtr));
        }
    }

    return res;
}

PhysicalModel::Surface::Multilayer* Parser::readMultilayerSurface(
        const MatId id,
        const std::string& name,
        const json& layers) {
    std::vector<Math::Real> thick, relEp, relMu, eCond, mCond;
    for (json::const_iterator it = layers.begin(); it != layers.end(); ++it) {
        thick.push_back( it->at("thickness").get<double>() );
        relEp.push_back( it->at("permittivity").get<double>() );
        relMu.push_back( it->at("permeability").get<double>() );
        eCond.push_back( it->at("elecCond").get<double>() );
        mCond.push_back( it->at("magnCond").get<double>() );
    }
    return new PhysicalModel::Surface::Multilayer(
            id, name, thick, relEp, relMu, eCond, mCond);
}

Geometry::Grid3 Parser::readGrids(const json& j) {
    if (j.find("grids") == j.end()) {
        throw std::logic_error("Grids object not found.");
    }

    json g = j.at("grids").front();
    std::string gridType = g.at("gridType").get<std::string>();
    if (gridType.compare("gridCondition") == 0) {
        // Initializes basic grid.
        Geometry::Grid3 res;
        if (g.at("type").get<std::string>().compare("by_number_of_cells") == 0) {
            res = Geometry::Grid3(
                    strToBox(g.at("layerBox").get<std::string>()),
                    strToCVecI3(g.at("directions").get<std::string>()));
        } else {
            res = Geometry::Grid3(
                    strToBox(g.at("layerBox").get<std::string>()),
                    strToCVecR3(g.at("directions").get<std::string>()));
        }

        // Applies boundary padding operations.
        std::pair<Math::CVecR3, Math::CVecR3> boundaryMeshSize(
                strToCVecR3(g.at("lowerPaddingMeshSize").get<std::string>()),
                strToCVecR3(g.at("upperPaddingMeshSize").get<std::string>()));
        std::pair<Math::CVecR3, Math::CVecR3> boundaryPadding(
                strToCVecR3(g.at("lowerPadding").get<std::string>()),
                strToCVecR3(g.at("upperPadding").get<std::string>()));
        if (g.at("boundaryPaddingType").get<std::string>().compare(
                "by_number_of_cells") == 0) {
            boundaryPadding.first  *= boundaryMeshSize.first;
            boundaryPadding.second *= boundaryMeshSize.second;
        }
        res.enlarge(boundaryPadding, boundaryMeshSize);
        return res;

    } else if (gridType.compare("nativeGiD") == 0) {
        std::vector<Math::Real> pos[3];
        pos[0] = g.at("xCoordinates").get<std::vector<double>>();
        pos[1] = g.at("yCoordinates").get<std::vector<double>>();
        pos[2] = g.at("zCoordinates").get<std::vector<double>>();
        return Geometry::Grid3(pos);

    } else {
        throw std::logic_error("Unrecognized grid type: " + gridType);
    }
}

Source::PlaneWave* Parser::readPlanewave(
        Geometry::Mesh::Geometric& mesh, const json& j) {

    Source::Magnitude::Magnitude magnitude =
            readMagnitude(j.at("magnitude").get<json>());
    Geometry::Element::Group<Geometry::Vol> elems =
            boxToElemGroup( mesh, j.at("layerBox").get<std::string>() );

    std::string definitionMode = j.at("definitionMode").get<std::string>();
    if (definitionMode.compare("by_vectors")==0) {
        Math::CVecR3 dir =
                strToCVecR3(j.at("directionVector").get<std::string>());
        Math::CVecR3 pol =
                strToCVecR3(j.at("polarizationVector").get<std::string>());
        return new Source::PlaneWave(magnitude, elems, dir, pol);

    } else if (definitionMode.compare("by_angles")==0) {
        static const Math::Real degToRad = 2.0 * Math::Constants::pi / 360.0;
        std::pair<Math::Real,Math::Real> dirAngles, polAngles;
        dirAngles.first  = j.at("directionTheta").get<double>()    * degToRad;
        dirAngles.second = j.at("directionPhi").get<double>()      * degToRad;
        polAngles.first  = j.at("polarizationAlpha").get<double>() * degToRad;
        polAngles.second = j.at("polarizationBeta").get<double>()  * degToRad;
        return new Source::PlaneWave(magnitude, elems, dirAngles, polAngles);

    } else if (definitionMode.compare("randomized_multisource")==0) {
        return new Source::PlaneWave(magnitude, elems,
                j.at("numberOfRandomPlanewaves").get<int>(),
                j.at("relativeVariationOfRandomDelay").get<double>());

    } else {
        throw std::logic_error("Unrecognized label: " + definitionMode);
    }
}

Source::Port::Waveguide* Parser::readPortWaveguide(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    std::string shape = j.at("shape").get<std::string>();
    if (shape.compare("Rectangular") == 0) {
        return new Source::Port::WaveguideRectangular(
                readMagnitude(       j.at("magnitude").get<json>()),
                readAsSurfaces(mesh, j.at("elemIds").get<json>()),
                strToWaveguideMode(  j.at("excitationMode").get<std::string>()),
                std::pair<Math::UInt,Math::UInt>(
                                     j.at("firstMode").get<int>(),
                                     j.at("secondMode").get<int>()) );
    } else {
        throw std::logic_error("Unrecognized waveguide port shape: " + shape);
    }
}

Source::Port::TEM* Parser::readPortTEM(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    return new Source::Port::TEMCoaxial(
            readMagnitude(       j.at("magnitude").get<json>()),
            readAsSurfaces(mesh, j.at("elemIds").get<json>()),
            strToTEMMode(        j.at("excitationMode").get<std::string>()),
            strToCVecR3(         j.at("origin").get<std::string>()),
                                 j.at("innerRadius").get<double>(),
                                 j.at("outerRadius").get<double>());
}

Source::Generator* Parser::readGenerator(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    return new Source::Generator(
            readMagnitude(     j.at("magnitude").get<json>() ),
            readAsNodes( mesh, j.at("coordIds").get<json>() ),
            strToGeneratorType(j.at("type").get<std::string>() ),
            strToNodalHardness(j.at("hardness").get<std::string>() ) );
}

Source::OnLine* Parser::readSourceOnLine(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    return new Source::OnLine(
            readMagnitude(     j.at("magnitude").get<json>()),
            readAsLines(mesh,  j.at("elemIds").get<json>()),
            strToNodalType(    j.at("type").get<std::string>()),
            strToNodalHardness(j.at("hardness").get<std::string>()) );
}

OutputRequest::Base::Type Parser::strToOutputType(std::string str) {
    str = trim(str);
    if (str.compare("electricField")==0) {
        return OutputRequest::Base::electric;
    } else if (str.compare("magneticField")==0) {
        return OutputRequest::Base::magnetic;
    } else if (str.compare("electricFieldNormals")==0) {
        return OutputRequest::Base::electricFieldNormals;
    } else if (str.compare("magneticFieldNormals")==0) {
        return OutputRequest::Base::magneticFieldNormals;
    } else if (str.compare("current")==0) {
        return OutputRequest::Base::current;
    } else if (str.compare("voltage")==0) {
        return OutputRequest::Base::voltage;
    } else if (str.compare("bulkCurrentElectric")==0) {
        return OutputRequest::Base::bulkCurrentElectric;
    } else if (str.compare("bulkCurrentMagnetic")==0) {
        return OutputRequest::Base::bulkCurrentMagnetic;
    } else if (str.compare("farField")==0) {
        return OutputRequest::Base::electric;
    } else {
        throw std::logic_error("Unrecognized output type: " + str);
    }
}

Source::Generator::Type Parser::strToGeneratorType(std::string str) {
    str = trim(str);
    if (str.compare("voltage")==0) {
        return Source::Generator::voltage;
    } else if (str.compare("current")==0) {
        return Source::Generator::current;
    } else {
        throw std::logic_error("Unrecognized generator type: " + str);
    }
}

Source::Generator::Hardness Parser::strToGeneratorHardness(std::string str) {
    str = trim(str);
    if (str.compare("soft")==0) {
        return Source::Generator::soft;
    } else if (str.compare("hard")==0) {
        return Source::Generator::hard;
    } else {
        throw std::logic_error("Unrecognized generator hardness: " + str);
    }
}

PhysicalModel::PhysicalModel::Type Parser::strToMaterialType(std::string str) {
    str = trim(str);
    if (str.compare("PEC")==0) {
        return PhysicalModel::PhysicalModel::PEC;
    } else if (str.compare("PMC")==0) {
        return PhysicalModel::PhysicalModel::PMC;
    } else if (str.compare("PML")==0) {
        return PhysicalModel::PhysicalModel::PML;
    } else if (str.compare("SMA")==0) {
        return PhysicalModel::PhysicalModel::SMA;
    } else if (str.compare("Classic")==0) {
        return PhysicalModel::PhysicalModel::classic;
    } else if (str.compare("Dispersive")==0) {
        return PhysicalModel::PhysicalModel::elecDispersive;
    } else if (str.compare("Anisotropic")==0) {
        return PhysicalModel::PhysicalModel::anisotropic;
    } else if (str.compare("SIBC")==0) {
        return PhysicalModel::PhysicalModel::isotropicsibc;
    } else if (str.compare("Wire")==0) {
        return PhysicalModel::PhysicalModel::wire;
    } else if (str.compare("Connector")==0) {
        return PhysicalModel::PhysicalModel::multiport;
    } else if (str.find("Thin_gap")==0) {
        return PhysicalModel::PhysicalModel::gap;
    } else {
        throw std::logic_error("Unrecognized material label: " + str);
    }
}

PhysicalModel::Multiport::Multiport::Type Parser::strToMultiportType(
        std::string str) {
    str = trim(str);
    if (str.compare("Conn_short")==0) {
        return PhysicalModel::Multiport::Multiport::shortCircuit;
    } else if (str.compare("Conn_open")==0) {
        return PhysicalModel::Multiport::Multiport::openCircuit;
    } else if (str.compare("Conn_matched")==0) {
        return PhysicalModel::Multiport::Multiport::matched;
    } else if (str.compare("Conn_sRLC")==0) {
        return PhysicalModel::Multiport::Multiport::sRLC;
    } else if (str.compare("Conn_pRLC")==0) {
        return PhysicalModel::Multiport::Multiport::pRLC;
    } else if (str.compare("Conn_sLpRC")==0) {
        return PhysicalModel::Multiport::Multiport::sLpRC;
    } else if (str.compare("Conn_dispersive") == 0) {
        return PhysicalModel::Multiport::Multiport::dispersive;
    } else {
        throw std::logic_error("Unrecognized multiport label: " + str);
    }
}

std::pair<Math::CVecR3, Math::CVecR3> Parser::strToBox(
        const std::string& value) {
    std::size_t begin = value.find_first_of("{");
    std::size_t end = value.find_last_of("}");
    std::string aux = value.substr(begin+1,end-1);
    std::stringstream iss(aux);
    Math::CVecR3 max, min;
    for (std::size_t i = 0; i < 3; i++) {
        iss >> max(i);
    }
    for (std::size_t i = 0; i < 3; i++) {
        iss >> min(i);
    }
    return std::pair<Math::CVecR3,Math::CVecR3>(min, max);
}

Math::CVecI3 Parser::strToCVecI3(const std::string& str) {
    std::stringstream ss(str);
    Math::CVecI3 res;
    ss >> res(Math::Constants::x)
       >> res(Math::Constants::y)
       >> res(Math::Constants::z);
    return res;
}

Math::CVecR3 Parser::strToCVecR3(const std::string& str) {
    std::stringstream ss(str);
    Math::CVecR3 res;
    ss >> res(Math::Constants::x)
       >> res(Math::Constants::y)
       >> res(Math::Constants::z);
    return res;
}

Source::OnLine::Type Parser::strToNodalType(std::string str) {
    str = trim(str);
    if (str.compare("electricField")==0) {
        return Source::OnLine::electric;
    } else if (str.compare("magneticField")==0) {
        return Source::OnLine::magnetic;
    } else {
        throw std::logic_error("Unrecognized nodal type: " + str);
    }
}

Source::OnLine::Hardness Parser::strToNodalHardness(std::string str) {
    str = trim(str);
    if (str.compare("soft")==0) {
        return Source::OnLine::soft;
    } else if (str.compare("hard")==0) {
        return Source::OnLine::hard;
    } else {
        throw std::logic_error("Unrecognized nodal hardness: " + str);
    }
}

Parser::OutputType Parser::strToGiDOutputType(std::string str) {
    str = trim(str);
    if (str.compare("OutRq_on_point")==0) {
        return Parser::outRqOnPoint;
    } else if (str.compare("OutRq_on_line")==0) {
        return Parser::outRqOnLine;
    } else if (str.compare("OutRq_on_surface")==0) {
        return Parser::outRqOnSurface;
    } else if (str.compare("OutRq_on_layer")==0) {
        return Parser::outRqOnLayer;
    } else if (str.compare("Bulk_current_on_surface")==0) {
        return Parser::bulkCurrentOnSurface;
    } else if (str.compare("Bulk_current_on_layer")==0) {
        return Parser::bulkCurrentOnLayer;
    } else if (str.compare("Far_field")==0) {
        return Parser::farField;
    } else {
        throw std::logic_error("Unrecognized label " + str);
    }
}

OutputRequest::Domain Parser::readDomain(const json& j) {
    bool timeDomain;
    Math::Real initialTime, finalTime, samplingPeriod;

    bool frequencyDomain, logFrequencySweep, usingTransferFunction;
    Math::Real initialFrequency, finalFrequency, frequencyStep;
    std::string transferFunctionFile;

    if (j.find("initialTime") != j.end()) {
        timeDomain     = true;
        initialTime    = j.at("initialTime").get<double>();
        finalTime      = j.at("finalTime").get<double>();
        samplingPeriod = j.at("samplingPeriod").get<double>();
    } else {
        timeDomain     = false;
    }

    if (j.find("initialFrequency") != j.end()) {
        frequencyDomain   = true;
        initialFrequency  = j.at("initialFrequency").get<double>();
        finalFrequency    = j.at("finalFrequency").get<double>();
        frequencyStep     = j.at("frequencyStep").get<double>();
        logFrequencySweep = j.at("logFrequencySweep").get<bool>();
        if (j.find("transferFunctionFile") != j.end()) {
            usingTransferFunction = true;
            transferFunctionFile =
                    j.at("transferFunctionFile").get<std::string>();
        } else {
            usingTransferFunction = false;
        }
    } else {
        frequencyDomain = false;
    }

    return OutputRequest::Domain(
            timeDomain, initialTime, finalTime, samplingPeriod,
            frequencyDomain, initialFrequency, finalFrequency,
            frequencyStep, logFrequencySweep,
            usingTransferFunction, transferFunctionFile);
}

Source::Magnitude::Magnitude* Parser::readMagnitude(const json& j) {
    std::string type = j.at("type").get<std::string>();
    if (type.compare("File") == 0) {
       return new Source::Magnitude::Numerical(
               j.at("filename").get<std::string>());
    } else if (type.compare("Gaussian") == 0) {
       return new Source::Magnitude::Magnitude(new Math::Function::Gaussian(
                       j.at("gaussianSpread").get<double>(),
                       j.at("gaussianDelay").get<double>()));
    } else {
        throw std::logic_error(
            "Unable to recognize magnitude type when reading excitation.");
    }
}

bool Parser::checkVersionCompatibility(const std::string& version) {
    bool versionMatches = (version == std::string(OPENSEMBA_VERSION));
    if (!versionMatches) {
        throw std::logic_error(
                "File version " + version + " is not supported.");
    }
    return versionMatches;
}

Math::Axis::Local Parser::strToLocalAxes(const std::string& str) {
    std::size_t begin = str.find_first_of("{");
    std::size_t end = str.find_first_of("}");
    Math::CVecR3 eulerAngles = strToCVecR3(str.substr(begin+1,end-1));
    begin = str.find_last_of("{");
    end = str.find_last_of("}");
    Math::CVecR3 origin = strToCVecR3(str.substr(begin+1,end-1));
    return Math::Axis::Local(eulerAngles, origin);
}

Parser::ParsedElementIds Parser::readElementIds(
        const std::string& str,
        size_t numberOfVertices) {
    Parser::ParsedElementIds res;
    std::stringstream ss(str);

    ss >> res.elemId >> res.mat >> res.layer;
    res.v.resize(8);
    for (std::size_t j = 0; j < numberOfVertices; j++) {
        ss >> res.v[j];
    }

    return res;
}

Parser::ParsedElementPtrs Parser::convertElementIdsToPtrs(
        const ParsedElementIds& elemIds,
        const PhysicalModel::Group<>& physicalModels,
        const Geometry::Layer::Group<>& layers,
        const Geometry::CoordR3Group& coords) {

    ParsedElementPtrs res;

    if (elemIds.mat != MatId(0)) {
        res.matPtr = physicalModels.getId(elemIds.mat);
    } else {
        res.matPtr = nullptr;
    }

    if (elemIds.layer != Geometry::LayerId(0)) {
        res.layerPtr = layers.getId(elemIds.layer);
    }
    else {
        res.layerPtr = nullptr;
    }

    res.vPtr.resize(elemIds.v.size(), nullptr);
    for (size_t i = 0; i < elemIds.v.size(); ++i) {
        res.vPtr[i] = coords.getId(elemIds.v[i]);
    }

    return res;
}

Source::Port::TEM::ExcitationMode Parser::strToTEMMode(std::string str) {
    if (str.compare("Voltage") == 0) {
        return Source::Port::TEM::voltage;
    } else if (str.compare("Current") == 0) {
        return Source::Port::TEM::current;
    } else {
        throw std::logic_error("Unrecognized exc. mode label: " + str);
    }

}

Source::Port::Waveguide::ExcitationMode Parser::strToWaveguideMode(
        std::string str) {
    if (str.compare("TE") == 0) {
        return Source::Port::Waveguide::TE;
    } else if (str.compare("TM") == 0) {
        return Source::Port::Waveguide::TM;
    } else {
        throw std::logic_error(
                "Unrecognized excitation mode: " + str);
    }
}

const PhysicalModel::Bound::Bound* Parser::strToBoundType(std::string str) {
    if (str.compare("PEC") == 0) {
        return new PhysicalModel::Bound::PEC(MatId(0));
    } else if (str.compare("PMC") == 0) {
        return new PhysicalModel::Bound::PMC(MatId(0));
    } else if (str.compare("PML") == 0) {
        return new PhysicalModel::Bound::PML(MatId(0));
    } else if (str.compare("Periodic") == 0) {
        return new PhysicalModel::Bound::Periodic(MatId(0));
    } else if (str.compare("MUR1") == 0) {
        return new PhysicalModel::Bound::Mur1(MatId(0));
    } else if (str.compare("MUR2") == 0) {
        return new PhysicalModel::Bound::Mur2(MatId(0));
    } else {
        throw std::logic_error("Unrecognized bound label: " + str);
    }
}

Geometry::Element::Group<Geometry::Nod> Parser::readAsNodes(
        Geometry::Mesh::Geometric& mesh, const json& j) {
    std::vector<Geometry::ElemId> nodeIds;
    for (auto it = j.begin(); it != j.end(); ++it) {
        Geometry::CoordId coordId( it->get<int>() );
        const Geometry::CoordR3* coord = mesh.coords().getId(coordId);
        Geometry::NodR* node = new Geometry::NodR(Geometry::ElemId(0), &coord);
        mesh.elems().addId(node);
        nodeIds.push_back(node->getId());
    }
    return mesh.elems().getId(nodeIds);
}

Geometry::Element::Group<const Geometry::Lin> Parser::readAsLines(
            Geometry::Mesh::Geometric& mesh, const json& j) {
    Geometry::Element::Group<const Geometry::Lin> surfs;
    for (auto it = j.begin(); it != j.end(); ++it) {
        surfs.add(mesh.elems().getId(Geometry::ElemId(it->get<int>())));
    }
    return surfs;
}

Geometry::Element::Group<const Geometry::Surf> Parser::readAsSurfaces(
            Geometry::Mesh::Geometric& mesh, const json& j) {
    Geometry::Element::Group<const Geometry::Surf> surfs;
    for (auto it = j.begin(); it != j.end(); ++it) {
        surfs.add(mesh.elems().getId(Geometry::ElemId(it->get<int>())));
    }
    return surfs;
}

} /* namespace JSON */
} /* namespace Parser */
} /* namespace SEMBA */
