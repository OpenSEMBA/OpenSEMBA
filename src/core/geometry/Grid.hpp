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
#include "Grid.h"

template<Int D>
const Real Grid<D>::tolerance = 1e-2;

template<Int D>
Grid<D>::Grid() {

}

template<Int D>
Grid<D>::Grid(const BoxRD& box,
        const CVecRD& dxyz) {
    CVecRD origin = box.getMin();
    offset_ = CVecID(0, 0, 0);
    for (Int i = 0; i < D; i++) {
        Real boxLength = box.getMax()(i) - box.getMin()(i);
        Int nCells;
        if (dxyz(i) == (Real) 0.0) {
            nCells = 1;
        } else {
            nCells = ceil(boxLength / dxyz(i));
            if (MathUtils::greater(boxLength, nCells*dxyz(i),
                    dxyz(i), tolerance)) {
                nCells++;
            }
        }
        pos_[i].resize(nCells+1);
        for (Int j = 0; j < nCells+1; j++) {
            pos_[i][j] = origin(i) + j * dxyz(i);
        }
    }
}

template<Int D>
Grid<D>::Grid(const BoxRD &boundingBox,
        const CVecID& dims) {
    CVecRD origin = boundingBox.getMin();
    offset_ = CVecID(0, 0, 0);
    for (Int i = 0; i < D; i++) {
        Real step =
                (boundingBox.getMax()(i) - boundingBox.getMin()(i)) / dims(i);
        Int nCells = dims(i);
        pos_[i].resize(nCells+1);
        for (Int j = 0; j < nCells+1; j++) {
            pos_[i][j] = origin(i) + j * step;
        }
    }
}

template<Int D>
Grid<D>::Grid(const vector<Real> step[D],
        const CVecID& offset,
        const CVecRD& origin) {
    offset_ = offset;
    for(Int d = 0; d < D; d++) {
        pos_[d].resize(step[d].size()+1);
        pos_[d][0] = origin(d);
        for (UInt i = 0; i < step[d].size(); i++) {
            pos_[d][i+1] = pos_[d][i] + step[d][i];
        }
    }
}


template<Int D>
Grid<D>::Grid(const Grid<D>& grid) {
    offset_ = grid.offset_;
    for (Int i = 0; i < D; i++) {
        pos_[i] = grid.pos_[i];
    }
}

template<Int D>
Grid<D>::~Grid() {

}

template<Int D>
Grid<D>& Grid<D>::operator=(const Grid<D>& rhs) {
    if (this == &rhs) {
        return *this;
    }
    offset_ = rhs.offset_;
    for (Int i = 0; i < D; i++) {
        pos_[i] = rhs.pos_[i];
    }
    return *this;
}

template<Int D>
void Grid<D>::setPos(const vector<Real> pos[D], const CVecID& offset) {
    offset_ = offset;
    for(Int d = 0; d < D; d++) {
        if (pos[d].size() == 0) {
            throw Error("Grid positions must contain at least one value");
        }
        pos_[d] = pos[d];
        if (pos_[d].size() == 1) {
            pos_[d].push_back(pos_[d][0]);
        }
    }
}

template<Int D>
void Grid<D>::setAdditionalSteps(
        const CartesianAxis d,
        const CartesianBound b,
        const vector<Real>& step) {
    const Int nCells = step.size();
    vector<Real> newPos(nCells);
    if (b == U) {
        newPos[0] = pos_[d].back() + step[0];
        for (Int i = 1; i < nCells; i++) {
            newPos[i] = newPos[i-1] + step[i];
        }
        pos_[d].insert(pos_[d].end(), newPos.begin(), newPos.end());
    } else {
        newPos[nCells-1] = pos_[d].front() - step[0];
        for (Int i = nCells-2; i >= 0 ; i--) {
            newPos[i] = newPos[i+1] - step[nCells-1-i];
        }
        newPos.insert(newPos.end(), pos_[d].begin(), pos_[d].end());
        pos_[d] = newPos;
    }
}

template<Int D>
bool Grid<D>::hasZeroSize() const {
    bool res = true;
    for (Int i = 0; i < D; i++) {
        res &= (pos_[i].size() <= 1);
    }
    return res;
}

template<Int D>
bool Grid<D>::isInto(const Int dir, const Real pos) const {
    if (pos >= getPos(dir)[0] && pos <= getPos(dir).back()) {
        return true;
    }
    return false;
}

//template<Int D>
//bool Grid<D>::isIntoDir(const CartesianAxis dir, const double val) const {
//    if(val < pos_[dir].front()){return false;}
//    if(val > pos_[dir].back()){return false;}
//    return true;
//}

template<Int D>
bool Grid<D>::getNaturalCellx(
        const double &x,long int &i, double &relativeLen) const {
    long int n = 0;
    relativeLen = -1.0;
    if (x < getPos(CartesianAxis::x,0)) {
        i = 0;
        return false;
    } else if (getPos(CartesianAxis::x,getNumCells()(CartesianAxis::x)) <= x) {
        i = getNumCells()(CartesianAxis::x);
        return false;
    }
    while (getPos(CartesianAxis::x)[n] <=x ){
        n++;
    }  /*mod this: use sort*/
    i = n-1;
    relativeLen = (x - getPos(CartesianAxis::x)[i])/getStep(CartesianAxis::x,i);
    return true;
}

template<Int D>
bool Grid<D>::getNaturalCelly(
        const double &y,long int &i, double &relativeLen) const {
    long int n = 0;
    relativeLen = -1.0;
    if (y < getPos(CartesianAxis::y,0)) {
        i = 0;
        return false;
    } else if (getPos(CartesianAxis::y,getNumCells()(CartesianAxis::y)) <= y) {
        i = getNumCells()(CartesianAxis::y);
        return false;
    }
    while (getPos(CartesianAxis::y,n) <= y) {
        n++;
    }  /*mod this: use sort*/
    i = n-1;
    relativeLen = (y-getPos(CartesianAxis::y,i)) / getStep(CartesianAxis::y,i);
    return true;
}

template<Int D>
bool Grid<D>::getNaturalCellz(
        const double &z,long int &i, double &relativeLen)const{
    long int n = 0;
    relativeLen = -1.0;
    if (z<getPos(CartesianAxis::z,0)) {
        i=0;
        return false;
    } else if(getPos(CartesianAxis::z,getNumCells()(CartesianAxis::z)) <=z ) {
        i=getNumCells()(CartesianAxis::z);
        return false;
    }
    while (getPos(CartesianAxis::z,n) <= z){
        n++;
    }  /*mod this: use sort*/
    i = n-1;
    relativeLen = (z-getPos(CartesianAxis::z,i)) / getStep(CartesianAxis::z,i);
    return true;
}

template<Int D>
bool Grid<D>::isInto(const CVecRD& pos) const {
    for (Int i = 0; i < 3; i++) {
        if (!isInto(i, pos(i))) {
            return false;
        }
    }
    return true;
}

template<Int D>
bool Grid<D>::isRegular() const {
    for (Int i = 0; i < D; i++) {
        if (!isRegular(i)) {
            return false;
        }
    }
    return true;
}

template<Int D>
bool Grid<D>::isRegular(const Int d) const {
    vector<Real> step = getStep(d);
    for (UInt n = 1; n < step.size(); n++) {
        if (MathUtils::notEqual(step[n], step[0], step[0], tolerance)) {
            return false;
        }
    }
    return true;
}

template<Int D>
bool Grid<D>::isCartesian() const {
    Real canon = getStep(x)[0];
    for (Int i = 0; i < D; i++) {
        vector<Real> step = getStep(i);
        for (UInt n = 1; n < step.size(); n++) {
            if (MathUtils::notEqual(step[n], canon, canon, tolerance)) {
                return false;
            }
        }
    }
    return true;
}

template<Int D>
bool Grid<D>::isCell(const CVecRD& position, const Real tol) const {
    pair<CVecID, CVecRD> natCell = getCellPair(position, true, tol);
    return natCell.second == CVecRD(0.0);
}

template<Int D>
bool Grid<D>::isCell(const vector<CVecRD>& pos, const Real tol) const {
    for (UInt i = 0; i < pos.size(); i++) {
        if (!isCell(pos[i], tol)) {
            return false;
        }
    }
    return true;
}

template<Int D>
CartesianVector<Int,D> Grid<D>::getNumCells() const {
    CVecID res;
    for (UInt d = 0; d < D; d++) {
        res(d) = getPos(d).size() - 1; // Minimum size of pos is 2.
    }
    return res;
}

template<Int D>
CartesianVector<Int,D> Grid<D>::getOffset() const {
    return offset_;
}

template<Int D>
CartesianVector<Real,D> Grid<D>::getOrigin() const {
    CVecRD res;
    for (UInt d = 0; d < D; d++) {
        if (pos_[d].size() == 0) {
            throw Error("Positions are not initialized.");
        }
        res(d) = pos_[d][0];
    }
    return res;
}

template<Int D>
vector<Real> Grid<D>::getStep(const Int dir) const {
    assert(dir >= 0 && dir < D);
    if (pos_[dir].size() == 0) {
        return vector<Real>();
    }
    vector<Real> res(pos_[dir].size()-1);
    for (UInt i = 0; i < pos_[dir].size()-1; i++) {
        res[i] = pos_[dir][i+1] - pos_[dir][i];
    }
    return res;
}


template<Int D>
Real Grid<D>::getStep(const Int dir, const Int& n) const {
    assert(dir >= 0 && dir < D);
    assert(n   >= 0 && n < (Int(pos_[dir].size()) - 1));

    if (pos_[dir].empty()) {
        return 0.0;
    }
    return pos_[dir][n+1] - pos_[dir][n];
}



template<Int D>
Real Grid<D>::getMinimumSpaceStep() const {
    Real minStep = numeric_limits<Real>::infinity();
    for (Int i = 0; i < D; i++) {
        vector<Real> step = getStep(i);
        for (UInt n = 0; n < step.size(); n++) {
            if (step[n] < minStep) {
                minStep = step[n];
            }
        }
    }
    return minStep;
}

template<Int D>
Box<Real,D> Grid<D>::getFullDomainBoundingBox() const {
    return getBoundingBox(
            pair<CVecID,CVecID> (offset_, offset_+getNumCells()) );
}

template<Int D>
Box<Int,D> Grid<D>::getFullDomainBoundingCellBox() const {

    CVecID min, max, dims;
    for (UInt n=0; n<D; n++){
        dims[n] = pos_[n].size();
    }

    BoxID res(offset_, offset_ + dims);
    return res;
}

template<Int D>
Box<Real,D> Grid<D>::getBoundingBox(const BoxID& bound) const {
    BoxRD res(getPos(bound.getMin()), getPos(bound.getMax()));
    return res;
}

template<Int D>
Box<Real,D> Grid<D>::getBoxRContaining(const CVecRD& point) const {
    BoxID boxI = getBoxIContaining(point);
    return getBoundingBox(boxI);
}


template<Int D>
Box<Int,D> Grid<D>::getBoxIContaining(const CVecRD& point) const {
    CVecID min = getCell(point, false);
    CVecID max = min + (Int)1;
    return BoxID(min, max);
}

template<Int D>
vector< CartesianVector<Real,D> > Grid<D>::getCenterOfCellsInside(
        const BoxRD& bound) const {
    // Determines centers of cells.
    vector<Real> center[D];
    for (Int dir = 0; dir < D; dir++) {
        vector<Real> pos = getPosInRange(dir,
                bound.getMin()(dir),
                bound.getMax()(dir));
        if (pos.size() > 0) {
            center[dir].reserve(pos.size()-1);
        }
        for (UInt i = 1; i < pos.size(); i++) {
            Real auxCenter = (pos[i-1] + pos[i]) / 2.0;
            center[dir].push_back(auxCenter);
        }
    }
    // Combines centers in a vector of CVecRD positions.
    vector<CVecRD> res;
    res.reserve(center[x].size() * center[y].size() * center[z].size());
    for (UInt i = 0; i < center[x].size(); i++) {
        for (UInt j = 0; j < center[y].size(); j++) {
            for (UInt k = 0; k < center[z].size(); k++) {
                res.push_back(CVecRD(center[x][i], center[y][j], center[z][k]));
            }
        }
    }
    return res;
}

template<Int D>
vector<Real> Grid<D>::getPosInRange(const Int dir,
        const Real min,
        const Real max) const {
    vector<Real> pos   = getPos (dir);
    vector<Real> steps = getStep(dir);
    vector<Real> res;
    res.reserve(pos.size());
    for (UInt i = 0; i < pos.size(); i++) {
        Real step;
        if (i < steps.size()) {
            step = steps[i];
        } else {
            step = steps.back();
        }
        if (MathUtils::equal(pos[i], min, step, tolerance) ||
                (pos[i] >= min && pos[i] <= max)               ||
                MathUtils::equal(pos[i], max, step, tolerance)) {
            res.push_back(pos[i]);
        }
    }
    return res;
}

template<Int D>
vector< CartesianVector<Real,D> > Grid<D>::getPos() const {
    // Combines positions in a vector of CVecRD positions.
    vector<CVecRD> res;
    res.reserve(pos_[x].size() * pos_[y].size() * pos_[z].size());
    for (UInt i = 0; i < pos_[x].size(); i++) {
        for (UInt j = 0; j < pos_[y].size(); j++) {
            for (UInt k = 0; k < pos_[z].size(); k++) {
                res.push_back(CVecRD(pos_[x][i], pos_[y][j], pos_[z][k]));
            }
        }
    }
    return res;
}

template<Int D>
vector<Real> Grid<D>::getPos(const Int direction) const {
    assert(direction >= 0 && direction < D);
    return pos_[direction];
};

template<Int D>
CartesianVector<Real,D> Grid<D>::getPos(const CVecID& ijk) const {
    CVecID dims = getNumCells();
    CVecRD res;
    for (Int i = 0; i < D; i++) {
//        assert((ijk(i) - offsetGrid_(i))>=0 &&
//                (ijk(i) - offsetGrid_(i))<dims(i));
        res(i) = pos_[i][ijk(i) - offset_(i)];
    }
    return res;
};

template<Int D>
Real Grid<D>::getPos(const Int dir, const Int i) const {
    return  pos_[(UInt)dir][i-offset_[dir]];
}

template<Int D>
pair<Int, Real> Grid<D>::getCellPair(const Int  dir,
        const Real x,
        const bool approx,
        const Real tol,
        bool* err) const {
    if (err != NULL) {
        *err = false;
    }

    Int  cell;
    Real dist;
    vector<Real> pos   = getPos (dir);
    vector<Real> steps = getStep(dir);
    assert(pos_[dir].size() >= 1);
    // Checks if it is below the grid.
    if (MathUtils::lower(x, pos[0], steps[0], tol)) {
        cell = offset_(dir);
        dist = (x-pos[0])/steps[0];
        if (err != NULL) {
            *err = true;
        }
        return make_pair(cell, dist);
    }
    for(UInt i = 0; i < pos.size(); i++) {
        Real step;
        if (i == 0) {
            step = steps.front();
        } else if (i <= steps.size()) {
            step = steps[i-1];
        } else {
            step = steps.back();
        }
        if (MathUtils::equal(x, pos[i], step, tol)) {
            cell = i + offset_(dir);
            dist = 0.0;
            if (err != NULL) {
                *err = false;
            }
            return make_pair(cell, dist);
        } else if (MathUtils::lower(x, pos[i], step, tol)) {
            cell = i - 1 + offset_(dir);
            dist = (x - pos[i-1])/step;
            if(MathUtils::equal(MathUtils::round(dist),1.0) && approx) {
                cell++;
                dist -= 1.0;
            }
            if (err != NULL) {
                *err = false;
            }
            return make_pair(cell, dist);
        }
    }
    cell = getNumCells()(dir) + offset_(dir);
    dist = (x - pos.back())/steps.back();
    if (err != NULL) {
        *err = true;
    }
    return make_pair(cell, dist);
}

template<Int D>
pair<CartesianVector<Int,D>, CartesianVector<Real,D> >
Grid<D>::getCellPair(const CVecRD& xyz,
        const bool approx,
        const Real tol,
        bool* err) const {
    if (err != NULL) {
        *err = false;
    }
    bool stepErr = false;

    CVecID cell;
    CVecRD dist;
    for (Int dir = 0; dir < D; dir++) {
        pair<Int, Real> res = getCellPair(dir,xyz(dir),approx,tol,&stepErr);
        cell(dir) = res.first;
        dist(dir) = res.second;
        if (err != NULL) {
            *err = *err || stepErr;
        }
    }
    return make_pair(cell, dist);
}

template<Int D>
CVecI3Fractional Grid<D>::getCVecI3Fractional (const CVecRD& xyz,
        bool& isInto) const{
    CVecI3 ijk   ;
    CVecR3 length;
    isInto = true  ;
    for(Int dir=0; dir<D; ++dir){
         if(!pos_[dir].empty()){
            if(xyz(dir) <= pos_[dir].front()){
                if(MathUtils::equal(pos_[dir].front(),xyz(dir))){
                    ijk(dir) = offset_[dir];
                    length(dir) = 0.0;
                }else{
                    isInto = false;
                    break;
                }
            }else if(pos_[dir].back()<=xyz(dir)) {
                if(MathUtils::equal(pos_[dir].back(),xyz(dir))){
                    ijk(dir) = offset_[dir]+pos_[dir].size()-1;
                    length(dir) = 0.0;
                }else{
                    isInto = false;
                    break;
                }
            }else{
                long int n = 0;
                while(pos_[dir][n] <= xyz(dir)){
                    ++n;
                }
                ijk(dir) = n-1 + offset_[dir];
                length(dir) = (xyz(dir)-pos_[dir][ijk(dir)])
                               /getStep(dir,ijk(dir));
            }
        }
    }
    return CVecI3Fractional (ijk,length);
}

template<Int D>
Int Grid<D>::getCell(const Int dir,
        const Real x,
        const bool approx,
        const Real tol,
        bool* err) const {
    return getCellPair(dir, x, approx, tol, err).first;
}

template<Int D>
CartesianVector<Int,D> Grid<D>::getCell(const CVecRD &coords,
        const bool approx,
        const Real tol,
        bool* err) const {
    return getCellPair(coords, approx, tol, err).first;
}

template<Int D>
void Grid<D>::applyScalingFactor(const Real factor) {
    for (Int d = 0; d < D; d++) {
        for (UInt i = 0; i < pos_[d].size(); i++) {
            pos_[d][i] *= factor;
        }
    }
}

template<Int D>
void Grid<D>::enlarge(const pair<CVecRD, CVecRD>& pad,
        const pair<CVecRD, CVecRD>& sizes) {
    for (Int d = 0; d < D; d++) {
        for (Int b = 0; b < 2; b++) {
            if (b == L) {
                enlargeBound(CartesianAxis(d), CartesianBound(b),
                        pad.first(d), sizes.first(d));
            } else {
                enlargeBound(CartesianAxis(d), CartesianBound(b),
                        pad.second(d), sizes.second(d));
            }
        }
    }
}

template<Int D>
void Grid<D>::enlargeBound(CartesianAxis d, CartesianBound b,
        Real pad, Real siz) {
    assert(getNumCells()(d) > 0);
    if (abs(siz) > abs(pad)) {
        cerr << "WARNING @ Grid enlarge bound: "
                << "Size was larger than padding. Ignoring padding in "
                << "axe" << d << " and bound " << b << "." << endl;
        return;
    }
    if (pad == 0.0) {
        return;
    }
    Int boundCell;
    if (b == L) {
        boundCell = 0;
    } else {
        boundCell = this->getNumCells()(d) - 1;
    }
    vector<Real> newSteps;
    if (MathUtils::greaterEqual(getStep(d,b), siz) || siz == 0.0) {
        siz = getStep(d,boundCell);
        // Computes enlargement for a padding with same size.
        Int nCells = (Int) MathUtils::ceil(abs(pad) / abs(siz), (Real) 0.01);
        newSteps.resize(nCells, siz);
    } else {
        // Computes enlargement for padding with different size.
        // Taken from AutoCAD interface programmed in LISP (2001).
        Real d12 = getStep(d,boundCell);
        Real d14 = abs(pad) + d12 + abs(siz);
        Real d34 = abs(siz);
        Real d13 = d14 - d34;
        Real t0 = d12;
        Real r0 = (d14-d12) / (d14-d34);
        Real r = r0;
        Int n = MathUtils::ceil(log(d34/d12) / log(r0), (Real) 0.01) - 1;
        if (n > 1) {
            // Newton method to adjust the sum of available space.
            Real f = 1;
            while (!MathUtils::equal(f, 0.0)) {
                f = t0 * (1-pow(r,n)) / (1-r) - d13;
                Real df = t0*(1-pow(r,n))/pow(1-r,2) - t0*n*pow(r,n-1)/(1-r);
                r = r - f / df;
            }
            newSteps.resize(n-1);
            for (Int i = 0; i < n-2; i++) {
                newSteps[i] = t0 * pow(r,(i+1));
            }
            newSteps[n-2] = d34;
        } else {
            newSteps.resize(1, d34);
        }
    }
    setAdditionalSteps(d, b, newSteps);
}

template<Int D>
void Grid<D>::printInfo() const {
    CVecID numCells = getNumCells();
    BoxRD bound = getFullDomainBoundingBox();
    cout << "-- Cartesian Grid<" << D << "> --" << endl;
    cout << "Offset: " << offset_.toStr() << endl;
    cout << "Dims: " << numCells.toStr() << endl;
    cout << "Min val: " << bound.getMin().toStr() << endl;
    cout << "Max val: " << bound.getMax().toStr() << endl;
}

template class Grid<1>;
template class Grid<2>;
template class Grid<3>;