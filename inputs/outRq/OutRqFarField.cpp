/*
 * OutputRequestFarField.cpp
 *
 *  Created on: Mar 5, 2015
 *      Author: luis
 */

#include "../outRq/OutRqFarField.h"

OutRqFarField::OutRqFarField(const Domain& domain,
                             const string& name,
                             const ElementsGroup<Volume<> >& elem,
                             const Real iTh, const Real fTh, const Real sTh,
                             const Real iPhi, const Real fPhi, const Real sPhi)
:   Domain(domain),
    OutRq<void>(electric, name),
    ElementsGroup<Volume<> >(elem) {

    setThetaAndPhi(iTh, fTh, sTh, iPhi, fPhi, sPhi);
}

OutRqFarField::OutRqFarField(const OutRqFarField& rhs)
:   Domain(rhs),
    OutRq<void>(rhs),
    ElementsGroup<Volume<> >(rhs) {

    initialTheta_ = rhs.initialTheta_;
    finalTheta_ = rhs.finalTheta_;
    stepTheta_ = rhs.stepTheta_;
    initialPhi_ = rhs.initialPhi_;
    finalPhi_ = rhs.finalPhi_;
    stepPhi_ = rhs.stepPhi_;
}

OutRqFarField::~OutRqFarField() {

}

Real OutRqFarField::getInitialTheta() const {
    return initialTheta_;
}

ClassBase* OutRqFarField::clone() const {
   return new OutRqFarField(*this);
}

Real OutRqFarField::getFinalTheta() const {
    return finalTheta_;
}

Real OutRqFarField::getStepTheta() const {
    return stepTheta_;
}

Real OutRqFarField::getInitialPhi() const {
    return initialPhi_;
}

Real OutRqFarField::getFinalPhi() const {
    return finalPhi_;
}

Real OutRqFarField::getStepPhi() const {
    return stepPhi_;
}

bool OutRqFarField::isSimilar(const OutRq<void>& rhs, const bool rev) const {
    if(!rhs.is<OutRqFarField>()) {
        return false;
    }
    const OutRqFarField* out = rhs.castTo<OutRqFarField>();
    bool isSimilar = OutRq<Vol>::isSimilar(rhs, true);
    isSimilar &= initialTheta_ == out->initialTheta_;
    isSimilar &= finalTheta_ == out->finalTheta_;
    isSimilar &= stepTheta_ == out->stepTheta_;
    isSimilar &= initialPhi_ == out->initialPhi_;
    isSimilar &= finalPhi_ == out->finalPhi_;
    isSimilar &= stepPhi_ == out->stepPhi_;
    if (!rev) {
        isSimilar &= rhs.isSimilar(rhs, true);
    }
    return isSimilar;
}

void OutRqFarField::setThetaAndPhi(Real iTh, Real fTh, Real sTh,
                                   Real iPhi, Real fPhi, Real sPhi) {
    initialTheta_ = iTh * acos(-1.0);
    finalTheta_ = fTh * acos(-1.0);
    stepTheta_ = sTh * acos(-1.0);
    initialPhi_ = iPhi * acos(-1.0);
    finalPhi_ = fPhi * acos(-1.0);
    stepPhi_ = sPhi* acos(-1.0);
}
