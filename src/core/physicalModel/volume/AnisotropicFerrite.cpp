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

#include <physicalModel/volume/AnisotropicFerrite.h>

namespace SEMBA {
namespace PhysicalModel {
namespace Volume {

AnisotropicFerrite::AnisotropicFerrite(
        const Id matId,
        const std::string& name,
        const Math::Axis::Local& local,
        const Math::Real kappa,
        const Math::Real relativePermeability,
        const Math::Real relativePermittivity)
:   Identifiable<Id>(matId),
    PhysicalModel(name),
    Anisotropic(local) {
    kappa_ = kappa;
    relativePermeability_ = relativePermeability;
    relativePermittivity_ = relativePermittivity;
}

AnisotropicFerrite::AnisotropicFerrite(
    const AnisotropicFerrite& rhs)
:   Identifiable<Id>(rhs),
    PhysicalModel(rhs),
    Anisotropic(rhs) {
    kappa_ = rhs.kappa_;
    relativePermeability_ = rhs.relativePermeability_;
    relativePermittivity_ = rhs.relativePermittivity_;
}

AnisotropicFerrite::~AnisotropicFerrite() {

}

Math::MatR33 AnisotropicFerrite::getRelPermittivityMatR() const {
    return Math::MatR33().setInDiagonal(Math::CVecR3(relativePermittivity_));
}

Math::MatR33 AnisotropicFerrite::getRelPermeabilityMatR() const {
    Math::MatR33 local;
    Math::CVecR3 principalAxis(relativePermeability_,
                               relativePermeability_, 1.0);
    local.setInDiagonal(principalAxis);
    return getLocalAxe().convertToGlobal(local);
}

Math::MatR33 AnisotropicFerrite::getRelPermeabilityMatI() const {
    Math::MatR33 local;
    local(0,1) =   kappa_;
    local(1,0) = - kappa_;
    return getLocalAxe().convertToGlobal(local);
}

Math::MatR33 AnisotropicFerrite::getElectricConductivityMat() const {
    return Math::MatR33();
}

Math::MatR33 AnisotropicFerrite::getMagneticConductivityMat() const {
    return Math::MatR33();
}

} /* namespace Volume */
} /* namespace PhysicalModel */
} /* namespace SEMBA */
