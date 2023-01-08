/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <dpsim-models/DP/DP_Ph1_RXLoad.h>

using namespace CPS;

DP::Ph1::RXLoad::RXLoad(String uid, String name, Logger::Level logLevel)
	: SimPowerComp<Complex>(uid, name, logLevel),
	mActivePower(Attribute<Real>::create("P", mAttributes)),
	mReactivePower(Attribute<Real>::create("Q", mAttributes)),
	mNomVoltage(Attribute<Real>::create("V_nom", mAttributes)) {
	setTerminalNumber(1);

	mSLog->info("Create {} {}", this->type(), name);
	**mIntfVoltage = MatrixComp::Zero(1, 1);
	**mIntfCurrent = MatrixComp::Zero(1, 1);
}

DP::Ph1::RXLoad::RXLoad(String name, Logger::Level logLevel)
	: RXLoad(name, name, logLevel) {
}

/// DEPRECATED: Delete method
SimPowerComp<Complex>::Ptr DP::Ph1::RXLoad::clone(String name) {
	auto copy = RXLoad::make(name, mLogLevel);
	if (mParametersSet)
		copy->setParameters(**mActivePower, **mReactivePower, **mNomVoltage);
	return copy;
}

void DP::Ph1::RXLoad::initializeFromNodesAndTerminals(Real frequency) {

	if(!mParametersSet){
		setParameters(
			mTerminals[0]->singleActivePower(),
			mTerminals[0]->singleReactivePower(),
			std::abs(mTerminals[0]->initialSingleVoltage()));
	}

	if (**mActivePower != 0) {
		mResistance = std::pow(**mNomVoltage, 2) / **mActivePower;
		mSubResistor = std::make_shared<DP::Ph1::Resistor>(**mName + "_res", mLogLevel);
		mSubResistor->setParameters(mResistance);
		mSubResistor->connect({ SimNode::GND, mTerminals[0]->node() });
		mSubResistor->initialize(mFrequencies);
		mSubResistor->initializeFromNodesAndTerminals(frequency);
		mSubComponents.push_back(mSubResistor);
	}
	else {
		mResistance = 0;
	}

	if (**mReactivePower != 0)
		mReactance = std::pow(**mNomVoltage, 2) / **mReactivePower;
	else
		mReactance = 0;

	if (mReactance > 0) {
		mInductance = mReactance / (2.*PI*frequency);
		mSubInductor = std::make_shared<DP::Ph1::Inductor>(**mName + "_ind", mLogLevel);
		mSubInductor->setParameters(mInductance);
		mSubInductor->connect({ SimNode::GND, mTerminals[0]->node() });
		mSubInductor->initialize(mFrequencies);
		mSubInductor->initializeFromNodesAndTerminals(frequency);
		mSubComponents.push_back(mSubInductor);
	}
	else if (mReactance < 0) {
		mCapacitance = -1. / (2.*PI*frequency) / mReactance;
		mSubCapacitor = std::make_shared<DP::Ph1::Capacitor>(**mName + "_cap", mLogLevel);
		mSubCapacitor->setParameters(mCapacitance);
		mSubCapacitor->connect({ SimNode::GND, mTerminals[0]->node() });
		mSubCapacitor->initialize(mFrequencies);
		mSubCapacitor->initializeFromNodesAndTerminals(frequency);
		mSubComponents.push_back(mSubCapacitor);
	}

	(**mIntfVoltage)(0, 0) = mTerminals[0]->initialSingleVoltage();
	(**mIntfCurrent)(0, 0) = std::conj(Complex(**mActivePower, **mReactivePower) / (**mIntfVoltage)(0, 0));

	mSLog->info(
		"\n--- Initialization from powerflow ---"
		"\nVoltage across: {:s}"
		"\nCurrent: {:s}"
		"\nTerminal 0 voltage: {:s}"
		"\nResistance: {:f}"
		"\nReactance: {:f}"
		"\n--- Initialization from powerflow finished ---",
		Logger::phasorToString((**mIntfVoltage)(0,0)),
		Logger::phasorToString((**mIntfCurrent)(0,0)),
		Logger::phasorToString(initialSingleVoltage(0)),
		mResistance,
		mReactance);
}

void DP::Ph1::RXLoad::setParameters(Real activePower, Real reactivePower, Real volt) {
	mParametersSet = true;
	**mActivePower = activePower;
	**mReactivePower = reactivePower;
	mPower = { **mActivePower, **mReactivePower};
	**mNomVoltage = volt;

	mSLog->info("Active Power={} [W] Reactive Power={} [VAr]", **mActivePower, **mReactivePower);
	mSLog->info("Nominal Voltage={} [V]", **mNomVoltage);
}

void DP::Ph1::RXLoad::mnaInitialize(Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
	MNAInterface::mnaInitialize(omega, timeStep);
	updateMatrixNodeIndices();

	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaInitialize(omega, timeStep, leftVector);
	}

	if (mSubInductor) {
		mRightVectorStamps.push_back(&**mSubInductor->mRightVector);
	}
	if (mSubCapacitor) {
		mRightVectorStamps.push_back(&**mSubCapacitor->mRightVector);
	}

	mMnaTasks.push_back(std::make_shared<MnaPreStep>(*this));
	mMnaTasks.push_back(std::make_shared<MnaPostStep>(*this, leftVector));
	**mRightVector = Matrix::Zero(leftVector->get().rows(), 1);
}

void DP::Ph1::RXLoad::mnaApplyRightSideVectorStamp(Matrix& rightVector) {
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaApplyRightSideVectorStamp(rightVector);
	}
}

void DP::Ph1::RXLoad::mnaApplySystemMatrixStamp(Matrix& systemMatrix) {
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaApplySystemMatrixStamp(systemMatrix);
	}
}

void DP::Ph1::RXLoad::mnaUpdateVoltage(const Matrix& leftVector) {
	(**mIntfVoltage)(0, 0) = Math::complexFromVectorElement(leftVector, matrixNodeIndex(0));
}

void DP::Ph1::RXLoad::mnaUpdateCurrent(const Matrix& leftVector) {
	(**mIntfCurrent)(0, 0) = 0;

	for (auto subComp : mSubComponents) {
		(**mIntfCurrent)(0, 0) += subComp->intfCurrent()(0, 0);
	}
}

void DP::Ph1::RXLoad::mnaAddPreStepDependencies(AttributeBase::List &prevStepDependencies, AttributeBase::List &attributeDependencies, AttributeBase::List &modifiedAttributes) {
	// add pre-step dependencies of subcomponents
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaAddPreStepDependencies(prevStepDependencies, attributeDependencies, modifiedAttributes);
	}

	// add pre-step dependencies of component itself
	modifiedAttributes.push_back(mRightVector);
}

void DP::Ph1::RXLoad::mnaPreStep(Real time, Int timeStepCount) {
	// pre-step of subcomponents
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaPreStep(time, timeStepCount);
	}

	// pre-step of component itself
	mnaApplyRightSideVectorStamp(**mRightVector);
}

void DP::Ph1::RXLoad::mnaAddPostStepDependencies(AttributeBase::List &prevStepDependencies, AttributeBase::List &attributeDependencies, AttributeBase::List &modifiedAttributes, Attribute<Matrix>::Ptr &leftVector) {
	// add post-step dependencies of subcomponents
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaAddPostStepDependencies(prevStepDependencies, attributeDependencies, modifiedAttributes, leftVector);
	}

	// add post-step dependencies of component itself
	attributeDependencies.push_back(leftVector);
	modifiedAttributes.push_back(mIntfVoltage);
	modifiedAttributes.push_back(mIntfCurrent);
}

void DP::Ph1::RXLoad::mnaPostStep(Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
	// post-step of subcomponents
	for (auto subComp : mSubComponents) {
		if (auto mnasubcomp = std::dynamic_pointer_cast<MNAInterface>(subComp))
			mnasubcomp->mnaPostStep(time, timeStepCount, leftVector);
	}

	// post-step of component itself
	mnaUpdateVoltage(**leftVector);
	mnaUpdateCurrent(**leftVector);
}
