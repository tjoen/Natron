/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

#ifndef KNOBIMPL_H
#define KNOBIMPL_H

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "Global/Macros.h"

#include "Knob.h"

#include <cfloat>
#include <stdexcept>
#include <string>
#include <algorithm> // min, max

#if !defined(Q_MOC_RUN) && !defined(SBK_RUN)
GCC_DIAG_UNUSED_LOCAL_TYPEDEFS_OFF
#include <boost/math/special_functions/fpclassify.hpp>
GCC_DIAG_UNUSED_LOCAL_TYPEDEFS_ON
#endif

#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

CLANG_DIAG_OFF(mismatched-tags)
GCC_DIAG_OFF(unused-parameter)
#include <shiboken.h>
CLANG_DIAG_ON(mismatched-tags)
GCC_DIAG_ON(unused-parameter)

#include "Engine/Curve.h"
#include "Engine/AppInstance.h"
#include "Engine/Project.h"
#include "Engine/EffectInstance.h"
#include "Engine/KnobTypes.h"
#include "Engine/Hash64.h"
#include "Engine/ViewIdx.h"
#include "Engine/EngineFwd.h"
#include "Engine/StringAnimationManager.h"


#define EXPR_RECURSION_LEVEL() KnobHelper::ExprRecursionLevel_RAII __recursionLevelIncrementer__(this)

NATRON_NAMESPACE_ENTER;

///template specializations

template <typename T>
void
Knob<T>::initMinMax()
{
}

template <>
void
KnobDoubleBase::initMinMax()
{
    for (int i = 0; i < getNDimensions(); ++i) {
        _minimums[i] = -DBL_MAX;
        _maximums[i] = DBL_MAX;
        _displayMins[i] = -DBL_MAX;
        _displayMaxs[i] = DBL_MAX;
    }
}

template <>
void
KnobIntBase::initMinMax()
{
    for (int i = 0; i < getNDimensions(); ++i) {
        _minimums[i] = INT_MIN;
        _maximums[i] = INT_MAX;
        _displayMins[i] = INT_MIN;
        _displayMaxs[i] = INT_MAX;
    }
}

template <typename T>
Knob<T>::Knob(const KnobHolderPtr& holder,
              const std::string & description,
              int dimension,
              bool declaredByPlugin )
    : KnobHelper(holder, description, dimension, declaredByPlugin)
    , _valueMutex(QMutex::Recursive)
    , _values(dimension)
    , _defaultValues(dimension)
    , _exprRes(dimension)
    , _minMaxMutex(QReadWriteLock::Recursive)
    , _minimums(dimension)
    , _maximums(dimension)
    , _displayMins(dimension)
    , _displayMaxs(dimension)
{
    initMinMax();
}

template <typename T>
Knob<T>::~Knob()
{
}

template <typename T>
void
Knob<T>::setRange(const T& mini,
                  const T& maxi,
                 DimSpec dimension)
{
    {
        QWriteLocker k(&_minMaxMutex);
        if (dimension.isAll()) {
            for (int i = 0; i < getNDimensions(); ++i) {
                _minimums[i] = mini;
                _maximums[i] = maxi;
            }
        } else {
            _minimums[dimension] = mini;
            _maximums[dimension] = maxi;
        }
    }

    _signalSlotHandler->s_minMaxChanged(dimension);
}

template <typename T>
void
Knob<T>::setDisplayRange(const T& mini,
                         const T& maxi,
                        DimSpec dimension)
{
    {
        QWriteLocker k(&_minMaxMutex);
        if (dimension.isAll()) {
            for (int i = 0; i < getNDimensions(); ++i) {
                _displayMins[i] = mini;
                _displayMaxs[i] = maxi;
            }
        } else {
            _displayMins[dimension] = mini;
            _displayMaxs[dimension] = maxi;
        }
    }

    _signalSlotHandler->s_displayMinMaxChanged(dimension);
}


template <typename T>
void
Knob<T>::setRangeAcrossDimensions(const std::vector<T> &minis,
                                const std::vector<T> &maxis)
{
    {
        QWriteLocker k(&_minMaxMutex);
        _minimums = minis;
        _maximums = maxis;
    }
    _signalSlotHandler->s_minMaxChanged(DimSpec::all());
}

template <typename T>
void
Knob<T>::setDisplayRangeAcrossDimensions(const std::vector<T> &minis,
                                       const std::vector<T> &maxis)
{
    {
        QWriteLocker k(&_minMaxMutex);
        _displayMins = minis;
        _displayMaxs = maxis;
    }
    _signalSlotHandler->s_displayMinMaxChanged(DimSpec::all());
}

template <typename T>
const std::vector<T>&
Knob<T>::getMinimums() const
{
    return _minimums;
}

template <typename T>
const std::vector<T>&
Knob<T>::getMaximums() const
{
    return _maximums;
}

template <typename T>
const std::vector<T>&
Knob<T>::getDisplayMinimums() const
{
    return _displayMins;
}

template <typename T>
const std::vector<T>&
Knob<T>::getDisplayMaximums() const
{
    return _displayMaxs;
}

template <typename T>
T
Knob<T>::getMinimum(DimIdx dimension) const
{
    QReadLocker k(&_minMaxMutex);

    return _minimums[dimension];
}

template <typename T>
T
Knob<T>::getMaximum(DimIdx dimension) const
{
    QReadLocker k(&_minMaxMutex);

    return _maximums[dimension];
}

template <typename T>
T
Knob<T>::getDisplayMinimum(DimIdx dimension) const
{
    QReadLocker k(&_minMaxMutex);

    return _displayMins[dimension];
}

template <typename T>
T
Knob<T>::getDisplayMaximum(DimIdx dimension) const
{
    QReadLocker k(&_minMaxMutex);

    return _displayMaxs[dimension];
}

template <typename T>
T
Knob<T>::clampToMinMax(const T& value,
                       DimIdx dimension) const
{
    if (dimension < 0 || dimension >= (int)_minimums.size()) {
        throw std::invalid_argument("Knob::clampToMinMax: dimension out of range");
    }
    QReadLocker k(&_minMaxMutex);

    return std::max( (double)_minimums[dimension], std::min( (double)_maximums[dimension], (double)value ) );
}

template <>
std::string
KnobStringBase::clampToMinMax(const std::string& value,
                              DimIdx /*dimension*/) const
{
    return value;
}

template <>
bool
KnobBoolBase::clampToMinMax(const bool& value,
                            DimIdx /*dimension*/) const
{
    return value;
}

template <>
int
KnobHelper::pyObjectToType(PyObject* o, ViewIdx /*view*/) const
{
    return (int)PyInt_AsLong(o);
}

template <>
bool
KnobHelper::pyObjectToType(PyObject* o, ViewIdx /*view*/) const
{
    if (PyObject_IsTrue(o) == 1) {
        return true;
    } else {
        return false;
    }
}

template <>
double
KnobHelper::pyObjectToType(PyObject* o, ViewIdx /*view*/) const
{
    return (double)PyFloat_AsDouble(o);
}

template <>
std::string
KnobHelper::pyObjectToType(PyObject* o, ViewIdx view) const
{
    if ( PyUnicode_Check(o) ) {
        std::string ret;
        PyObject* utf8pyobj = PyUnicode_AsUTF8String(o); // newRef
        if (utf8pyobj) {
            char* cstr = PyBytes_AS_STRING(utf8pyobj); // Borrowed pointer
            ret.append(cstr);
            Py_DECREF(utf8pyobj);
        }

        return ret;
    } else if ( PyString_Check(o) ) {
        return std::string( PyString_AsString(o) );
    }

    int index = 0;
    if ( PyFloat_Check(o) ) {
        index = std::floor( (double)PyFloat_AsDouble(o) + 0.5 );
    } else if ( PyLong_Check(o) ) {
        index = (int)PyInt_AsLong(o);
    } else if (PyObject_IsTrue(o) == 1) {
        index = 1;
    }

    const AnimatingKnobStringHelper* isStringAnimated = dynamic_cast<const AnimatingKnobStringHelper* >(this);
    if (!isStringAnimated) {
        return std::string();
    }
    std::string ret;
    isStringAnimated->stringFromInterpolatedValue(index, view, &ret);

    return ret;
}

inline unsigned int
hashFunction(unsigned int a)
{
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);

    return a;
}

template <typename T>
bool
Knob<T>::evaluateExpression(double time,
                            ViewIdx view,
                            DimIdx dimension,
                            T* value,
                            std::string* error)
{
    PythonGILLocker pgl;
    PyObject *ret;

    ///Reset the random state to reproduce the sequence
    randomSeed( time, hashFunction(dimension) );
    bool exprOk = executeExpression(time, view, dimension, &ret, error);
    if (!exprOk) {
        return false;
    }
    *value =  pyObjectToType<T>(ret, view);
    Py_DECREF(ret); //< new ref
    return true;
}

template <typename T>
bool
Knob<T>::evaluateExpression_pod(double time,
                                ViewIdx view,
                                DimIdx dimension,
                                double* value,
                                std::string* error)
{
    PythonGILLocker pgl;
    PyObject *ret;

    ///Reset the random state to reproduce the sequence
    randomSeed( time, hashFunction(dimension) );
    bool exprOk = executeExpression(time, view, dimension, &ret, error);
    if (!exprOk) {
        return false;
    }

    if ( PyFloat_Check(ret) ) {
        *value =  (double)PyFloat_AsDouble(ret);
    } else if ( PyInt_Check(ret) ) {
        *value = (int)PyInt_AsLong(ret);
    } else if ( PyLong_Check(ret) ) {
        *value = (int)PyLong_AsLong(ret);
    } else if (PyObject_IsTrue(ret) == 1) {
        *value = 1;
    } else {
        //Strings should always fall here
        *value = 0.;
    }

    return true;
}



template <typename T>
void
Knob<T>::valueToVariant(const T & v,
                        Variant* vari)
{
    KnobIntBase* isInt = dynamic_cast<KnobIntBase*>(this);
    KnobBoolBase* isBool = dynamic_cast<KnobBoolBase*>(this);
    KnobDoubleBase* isDouble = dynamic_cast<KnobDoubleBase*>(this);
    if (isInt) {
        vari->setValue(v);
    } else if (isBool) {
        vari->setValue(v);
    } else if (isDouble) {
        vari->setValue(v);
    }
}

template <>
void
KnobStringBase::valueToVariant(const std::string & v,
                                  Variant* vari)
{
    KnobStringBase* isString = dynamic_cast<KnobStringBase*>(this);
    if (isString) {
        vari->setValue<QString>( QString::fromUtf8( v.c_str() ) );
    }
}
template <typename T>
void
Knob<T>::makeKeyFrame(double time,
                      const T& v,
                      ViewIdx /*view*/,
                      KeyFrame* key)
{
    AnimatingObjectI::KeyframeDataTypeEnum type = getKeyFrameDataType();
    double keyFrameValue;
    switch (type) {
        case AnimatingObjectI::eKeyframeDataTypeBool:
            keyFrameValue = (bool)v;
            break;
        case AnimatingObjectI::eKeyframeDataTypeInt:
            keyFrameValue = std::floor(v + 0.5);
            break;
        default:
            keyFrameValue = (double)v;
            break;
    }

    // check for NaN or infinity
    if ( (keyFrameValue != keyFrameValue) || boost::math::isinf(keyFrameValue) ) {
        *key = KeyFrame( (double)time, getMaximum(0) );
    } else {
        *key = KeyFrame( (double)time, keyFrameValue );
    }
}

template <>
void
KnobStringBase::makeKeyFrame(double time,
                             const std::string& v,
                             ViewIdx view,
                             KeyFrame* key)
{
    double keyFrameValue = 0.;
    AnimatingKnobStringHelper* isStringAnimatedKnob = dynamic_cast<AnimatingKnobStringHelper*>(this);
    assert(isStringAnimatedKnob);
    if (isStringAnimatedKnob) {
        isStringAnimatedKnob->stringToKeyFrameValue(time, view, v, &keyFrameValue);
    }

    *key = KeyFrame( (double)time, keyFrameValue );
}


template<typename T>
void
Knob<T>::unSlaveInternal(DimIdx dimension,
                         ViewIdx view,
                         ValueChangedReasonEnum reason,
                         bool copyState)
{

    MasterKnobLink linkData;
    if (!getMaster(dimension, view, &linkData)) {
        return;
    }
    KnobHelperPtr masterHelper = boost::dynamic_pointer_cast<KnobHelper>(linkData.masterKnob.lock());
    if (!masterHelper) {
        return;
    }
    QObject::disconnect( masterHelper->_signalSlotHandler.get(), SIGNAL(curveAnimationChanged(std::list<double>, std::list<double>,ViewIdx,DimIdx)),
                         _signalSlotHandler.get(), SLOT(onMasterCurveAnimationChanged(std::list<double>, std::list<double>,ViewIdx,DimIdx)));



    resetMaster(dimension, view);
    bool hasChanged = false;
    setEnabled(dimension, true);
    if (copyState) {
        // Clone the master
        hasChanged |= copyKnob( masterHelper, view, linkData.masterView, dimension, linkData.masterDimension );
    }

    if (reason == eValueChangedReasonPluginEdited) {
        _signalSlotHandler->s_knobSlaved(dimension, view, false);
    }
    
    if (getHolder()) {
        getHolder()->onKnobSlaved( shared_from_this(), linkData.masterDimension, dimension, false );
    }
    if (masterHelper) {
        masterHelper->removeListener(shared_from_this(), dimension);
    }
    if (!hasChanged) {
        // At least refresh animation level if clone did not change anything
        refreshAnimationLevel(view, dimension);
    }
}

template<typename T>
T
Knob<T>::getDefaultValue(DimIdx dimension) const
{
    if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
        throw std::invalid_argument("Knob::getDefaultValue: Invalid dimension");
    }
    QMutexLocker l(&_valueMutex);

    return _defaultValues[dimension].value;
}

template<typename T>
T
Knob<T>::getInitialDefaultValue(DimIdx dimension) const
{
    if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
        throw std::invalid_argument("Knob::getInitialDefaultValue: Invalid dimension");
    }
    QMutexLocker l(&_valueMutex);

    return _defaultValues[dimension].initialValue;

}

template<typename T>
void
Knob<T>::setCurrentDefaultValueAsInitialValue()
{
    int nDims = getNDimensions();
    QMutexLocker l(&_valueMutex);
    for (int i = 0; i < nDims; ++i) {
        _defaultValues[i].initialValue = _defaultValues[i].value;
        _defaultValues[i].defaultValueSet = true;
    }
}

template<typename T>
bool
Knob<T>::hasDefaultValueChanged(DimIdx dimension) const
{
    if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
        throw std::invalid_argument("Knob::hasDefaultValueChanged: Invalid dimension");
    }
    QMutexLocker l(&_valueMutex);
    return _defaultValues[dimension].initialValue != _defaultValues[dimension].value;
}

template<typename T>
bool
Knob<T>::isDefaultValueSet(DimIdx dimension) const
{
    if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
        throw std::invalid_argument("Knob::isDefaultValueSet: Invalid dimension");
    }
    QMutexLocker l(&_valueMutex);
    return _defaultValues[dimension].defaultValueSet;
}

template<typename T>
void
Knob<T>::setDefaultValue(const T & v,
                         DimSpec dimension)
{
    {
        QMutexLocker l(&_valueMutex);
        if (dimension.isAll()) {
            int nDims = getNDimensions();
            for (int i = 0; i < nDims; ++i) {
                _defaultValues[i].value = v;
                if (!_defaultValues[i].defaultValueSet) {
                    _defaultValues[i].defaultValueSet = true;
                    _defaultValues[i].initialValue = v;
                }
            }
        } else {
            if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
                throw std::invalid_argument("Knob::setDefaultValue: Invalid dimension");
            }
            _defaultValues[dimension].value = v;
            if (!_defaultValues[dimension].defaultValueSet) {
                _defaultValues[dimension].defaultValueSet = true;
                _defaultValues[dimension].initialValue = v;
            }
        }
    }
    resetToDefaultValue(dimension, ViewSetSpec::all());
    computeHasModifications();
}

template<typename T>
void
Knob<T>::setDefaultValues(const std::vector<T>& values, DimIdx dimensionStartOffset)
{
    if (values.empty()) {
        return;
    }
    int nDims = getNDimensions();
    assert(nDims >= (int)values.size());
    if (dimensionStartOffset < 0 || dimensionStartOffset + (int)values.size() > nDims) {
        throw std::invalid_argument("Knob<T>::setDefaultValuesWithoutApplying: Invalid arguments");
    }
    QMutexLocker l(&_valueMutex);
    for (std::size_t i = 0; i < values.size(); ++i) {
        int dimension = i + dimensionStartOffset;
        _defaultValues[dimension].value = values[i];
        if (!_defaultValues[dimension].defaultValueSet) {
            _defaultValues[dimension].defaultValueSet = true;
            _defaultValues[dimension].initialValue = values[i];
        }

    }
    computeHasModifications();


}

template <typename T>
void
Knob<T>::setDefaultValueWithoutApplying(const T& v,
                                        DimSpec dimension)
{
    assert( dimension < getNDimensions() );
    {
        QMutexLocker l(&_valueMutex);
        if (dimension.isAll()) {
            int nDims = getNDimensions();
            for (int i = 0; i < nDims; ++i) {
                _defaultValues[i].value = v;
                if (!_defaultValues[i].defaultValueSet) {
                    _defaultValues[i].defaultValueSet = true;
                    _defaultValues[i].initialValue = v;
                }
            }
        } else {
            if (dimension < 0 || dimension >= (int)_defaultValues.size()) {
                throw std::invalid_argument("Knob::setDefaultValueWithoutApplying: Invalid dimension");
            }
            _defaultValues[dimension].value = v;
            if (!_defaultValues[dimension].defaultValueSet) {
                _defaultValues[dimension].defaultValueSet = true;
                _defaultValues[dimension].initialValue = v;
            }
        }

    }
    computeHasModifications();
}

template <typename T>
void
Knob<T>::setDefaultValuesWithoutApplying(const std::vector<T>& values, DimIdx dimensionStartOffset)
{
    if (values.empty()) {
        return;
    }
    int nDims = getNDimensions();
    assert(nDims >= (int)values.size());
    if (dimensionStartOffset < 0 || dimensionStartOffset + (int)values.size() > nDims) {
        throw std::invalid_argument("Knob<T>::setDefaultValuesWithoutApplying: Invalid arguments");
    }

    QMutexLocker l(&_valueMutex);
    for (std::size_t i = 0; i < values.size(); ++i) {
        int dimension = i + dimensionStartOffset;
        _defaultValues[dimension].value = values[i];
        if (!_defaultValues[dimension].defaultValueSet) {
            _defaultValues[dimension].defaultValueSet = true;
            _defaultValues[dimension].initialValue = values[i];
        }

    }
    computeHasModifications();

} // setDefaultValuesWithoutApplying

template <typename T>
void initDefaultValue(T* value)
{
    *value = T();
}


template <>
void initDefaultValue(double* value)
{
    *value = 0.;
}

template <>
void initDefaultValue(int* value)
{
    *value = 0;
}

template <>
void initDefaultValue(bool* value)
{
    *value = false;
}


template<>
void
Knob<bool>::populate()
{
    for (int i = 0; i < getNDimensions(); ++i) {
        _values[i][ViewIdx(0)] = false;
        _defaultValues[i].value = false;
        _defaultValues[i].defaultValueSet = false;
    }
    KnobHelper::populate();
}


template<typename T>
void
Knob<T>::populate()
{
    for (int i = 0; i < getNDimensions(); ++i) {
        initDefaultValue<T>(&_values[i][ViewIdx(0)]);
        initDefaultValue<T>(&_defaultValues[i].value);
        _defaultValues[i].defaultValueSet = false;
    }
    KnobHelper::populate();
}

template<typename T>
bool
Knob<T>::isTypePOD() const
{
    return false;
}

template<>
bool
KnobIntBase::isTypePOD() const
{
    return true;
}

template<>
bool
KnobBoolBase::isTypePOD() const
{
    return true;
}

template<>
bool
KnobDoubleBase::isTypePOD() const
{
    return true;
}

template<typename T>
bool
Knob<T>::isTypeCompatible(const KnobIPtr & other) const
{
    return isTypePOD() == other->isTypePOD();
}

template<typename T>
void
Knob<T>::splitView(ViewIdx view)
{
    {
        QMutexLocker k(&_valueMutex);
        int nDims = getNDimensions();
        for (int i = 0; i < nDims; ++i) {
            {
                const T& mainViewVal = _values[i][ViewIdx(0)];
                T& thisViewVal = _values[i][view];
                thisViewVal = mainViewVal;
            }
        }
    }
    KnobHelper::splitView(view);

}

template<typename T>
void
Knob<T>::unSplitView(ViewIdx view)
{
    {
        QMutexLocker k(&_valueMutex);
        int nDims = getNDimensions();
        for (int i = 0; i < nDims; ++i) {

            typename PerViewValueMap::iterator foundView = _values[i].find(view);
            if (foundView != _values[i].end()) {
                _values[i].erase(foundView);
            }


        }
    }
    KnobHelper::unSplitView(view);
    
}

template<typename T>
void
Knob<T>::onTimeChanged(bool isPlayback,
                       double time)
{
    int dims = getNDimensions();

    if ( getIsSecret() ) {
        return;
    }
    bool shouldRefresh = false;
    for (int i = 0; i < dims; ++i) {
        if (isAnimated( i, ViewIdx(0) )) {
            shouldRefresh = true;
        }
    }

    if (shouldRefresh) {
        refreshAnimationLevel(ViewIdx(0), -1);
        _signalSlotHandler->s_mustRefreshKnobGui(ViewSetSpec::all(), DimSpec::all(), eValueChangedReasonTimeChanged);
    }
    if (evaluateValueChangeOnTimeChange() && !isPlayback) {
        KnobHolderPtr holder = getHolder();
        if (holder) {
            //Some knobs like KnobFile do not animate but the plug-in may need to know the time has changed
            if ( holder->isEvaluationBlocked() ) {
                holder->appendValueChange(shared_from_this(), -1, time, ViewIdx(0), eValueChangedReasonTimeChanged, eValueChangedReasonTimeChanged);
            } else {
                holder->beginChanges();
                holder->appendValueChange(shared_from_this(), -1, time, ViewIdx(0), eValueChangedReasonTimeChanged, eValueChangedReasonTimeChanged);
                holder->endChanges();
            }
        }
    }
}

template<typename T>
double
Knob<T>::getDerivativeAtTime(double time,
                             ViewGetSpec view,
                             DimIdx dimension)
{
    if ( ( dimension > getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getDerivativeAtTime(): Dimension out of range");
    }
    {
        std::string expr = getExpression(dimension);
        if ( !expr.empty() ) {
            // Compute derivative by finite differences, using values at t-0.5 and t+0.5
            return ( getValueAtTime(time + 0.5, dimension, view) - getValueAtTime(time - 0.5, dimension, view) ) / 2.;
        }
    }

    // If the knob is slaved to another knob, returns the other knob value
    MasterKnobLink linkData;
    if (getMaster(dimension, &linkData)) {
        KnobIPtr masterKnob = linkData.masterKnob.lock();
        if (masterKnob) {
            return masterKnob->getDerivativeAtTime(time, linkData.masterView, linkData.masterDimension);
        }
    }

    CurvePtr curve  = getCurve(view, dimension);
    if (curve->getKeyFramesCount() > 0) {
        return curve->getDerivativeAt(time);
    } else {
        /*if the knob as no keys at this dimension, the derivative is 0.*/
        return 0.;
    }
}

template<>
double
KnobStringBase::getDerivativeAtTime(double /*time*/,
                                       ViewGetSpec /*view*/,
                                       DimIdx /*dimension*/)
{
    throw std::invalid_argument("Knob<string>::getDerivativeAtTime() not available");
}

// Compute integral using Simpsons rule:
// \int_a^b f(x) dx = (b-a)/6 * (f(a) + 4f((a+b)/2) + f(b))
template<typename T>
double
Knob<T>::getIntegrateFromTimeToTimeSimpson(double time1,
                                           double time2,
                                           ViewGetSpec view,
                                           DimIdx dimension)
{
    double fa = getValueAtTime(time1, dimension, view);
    double fm = getValueAtTime( (time1 + time2) / 2, dimension, view );
    double fb = getValueAtTime(time2, dimension, view);

    return (time2 - time1) / 6 * (fa + 4 * fm + fb);
}

template<typename T>
double
Knob<T>::getIntegrateFromTimeToTime(double time1,
                                    double time2,
                                    ViewGetSpec view,
                                    DimIdx dimension)
{
    if ( ( dimension > getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getIntegrateFromTimeToTime(): Dimension out of range");
    }
    {
        std::string expr = getExpression(dimension);
        if ( !expr.empty() ) {
            // Compute integral using Simpsons rule:
            // \int_a^b f(x) dx = (b-a)/6 * (f(a) + 4f((a+b)/2) + f(b))
            // The interval from time1 to time2 is split into intervals where bounds are at integer values
            int i = std::ceil(time1);
            int j = std::floor(time2);
            if (i > j) { // no integer values in the interval
                return getIntegrateFromTimeToTimeSimpson(time1, time2, view, dimension);
            }
            double val = 0.;
            // start integrating over the interval
            // first chunk
            if (time1 < i) {
                val += getIntegrateFromTimeToTimeSimpson(time1, i, view, dimension);
            }
            // integer chunks
            for (int t = i; t < j; ++t) {
                val += getIntegrateFromTimeToTimeSimpson(t, t + 1, view, dimension);
            }
            // last chunk
            if (j < time2) {
                val += getIntegrateFromTimeToTimeSimpson(j, time2, view, dimension);
            }

            return val;
        }
    }


    // If the knob is slaved to another knob, returns the other knob value
    MasterKnobLink linkData;
    if (getMaster(dimension, &linkData)) {
        KnobIPtr masterKnob = linkData.masterKnob.lock();
        if (masterKnob) {
            return masterKnob->getIntegrateFromTimeToTime(time1, time2, linkData.masterView, linkData.masterDimension);
        }
    }


    CurvePtr curve  = getCurve(view, dimension);
    if (curve->getKeyFramesCount() > 0) {
        return curve->getIntegrateFromTo(time1, time2);
    } else {
        // if the knob as no keys at this dimension, the integral is trivial
        QMutexLocker l(&_valueMutex);

        return (double)_values[dimension] * (time2 - time1);
    }
} // >::getIntegrateFromTimeToTime

template<>
double
KnobStringBase::getIntegrateFromTimeToTimeSimpson(double /*time1*/,
                                                     double /*time2*/,
                                                     ViewGetSpec /*view*/,
                                                     DimIdx /*dimension*/)
{
    return 0; // dummy value
}

template<>
double
KnobStringBase::getIntegrateFromTimeToTime(double /*time1*/,
                                              double /*time2*/,
                                              ViewGetSpec /*view*/,
                                              DimIdx /*dimension*/)
{
    throw std::invalid_argument("Knob<string>::getIntegrateFromTimeToTime() not available");
}


template <typename T>
void
Knob<T>::resetToDefaultValue(DimSpec dimension, ViewSetSpec view)
{
    removeAnimation(view, dimension);



    clearExpression(dimension, view, true);
    resetExtraToDefaultValue(dimension, view);

    int nDims = getNDimensions();
    std::vector<double> defValues(nDims);
    for (int i = 0; i < nDims; ++i) {
        if (dimension.isAll() || i == dimension) {
            {
                QMutexLocker l(&_valueMutex);
                defValues[i] = _defaultValues[i].value;
            }
        }
    }
    if (dimension.isAll()) {
        setValueAcrossDimensions(defValues, DimIdx(0), view, eValueChangedReasonRestoreDefault);
    } else {
        ignore_result( setValue(defValues[dimension], view, dimension, eValueChangedReasonRestoreDefault, NULL) );
    }

}


// If *all* the following conditions hold:
// - this is a double value
// - this is a non normalised spatial double parameter, i.e. kOfxParamPropDoubleType is set to one of
//   - kOfxParamDoubleTypeX
//   - kOfxParamDoubleTypeXAbsolute
//   - kOfxParamDoubleTypeY
//   - kOfxParamDoubleTypeYAbsolute
//   - kOfxParamDoubleTypeXY
//   - kOfxParamDoubleTypeXYAbsolute
// - kOfxParamPropDefaultCoordinateSystem is set to kOfxParamCoordinatesNormalised
// Knob<T>::resetToDefaultValue should denormalize
// the default value, using the input size.
// Input size be defined as the first available of:
// - the RoD of the "Source" clip
// - the RoD of the first non-mask non-optional input clip (in case there is no "Source" clip) (note that if these clips are not connected, you get the current project window, which is the default value for GetRegionOfDefinition)

// see http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamPropDefaultCoordinateSystem
// and http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#APIChanges_1_2_SpatialParameters
template<>
void
KnobDoubleBase::resetToDefaultValue(DimSpec dimension, ViewSetSpec view)
{
    removeAnimation(view, dimension);

    ///A KnobDoubleBase is not always a KnobDouble (it can also be a KnobColor)
    KnobDouble* isDouble = dynamic_cast<KnobDouble*>(this);

    clearExpression(dimension, view, true);


    resetExtraToDefaultValue(dimension, view);

    if (isDouble) {
        double time = getCurrentTime();

        // see http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamPropDefaultCoordinateSystem
        int nDims = getNDimensions();
        std::vector<double> defValues(nDims);
        for (int i = 0; i < nDims; ++i) {
            if (dimension.isAll() || i == dimension) {
                {
                    QMutexLocker l(&_valueMutex);
                    defValues[i] = _defaultValues[i].value;
                }

                if ( isDouble->getDefaultValuesAreNormalized() ) {
                    if (isDouble->getValueIsNormalized(DimIdx(i)) == eValueIsNormalizedNone) {
                        // default is normalized, value is non-normalized: denormalize it!
                        defValues[i] = isDouble->denormalize(DimIdx(i), time, defValues[i]);
                    }
                } else {
                    if (isDouble->getValueIsNormalized(DimIdx(i)) != eValueIsNormalizedNone) {
                        // default is non-normalized, value is normalized: normalize it!
                        defValues[i] = isDouble->normalize(DimIdx(i), time, defValues[i]);
                    }
                }

            }
        }
        if (dimension.isAll()) {
            setValueAcrossDimensions(defValues, DimIdx(0), view, eValueChangedReasonRestoreDefault);
        } else {
            ignore_result( setValue(defValues[dimension], view, dimension, eValueChangedReasonRestoreDefault, NULL) );
        }
    }


    
}

template <typename T>
typename Knob<T>::PerDimensionValuesVec
Knob<T>::getRawValues() const
{
    QMutexLocker ok(&_valueMutex);
    return _values;
}

template <typename T>
T
Knob<T>::getRawValue(DimIdx dimension, ViewIdx view) const
{
    if (dimension < 0 && dimension >= getNDimensions()) {
        throw std::invalid_argument("Knob::getRawValue: dimension out of range");
    }
    QMutexLocker ok(&_valueMutex);
    typename PerViewValueMap::const_iterator foundView = _values[dimension].find(view);
    if (foundView == _values[dimension].end()) {
        return T();
    }
    return foundView->second;
}


template<typename T>
template<typename OTHERTYPE>
bool
Knob<T>::copyValueForType(const boost::shared_ptr<Knob<OTHERTYPE> >& other,
                          ViewIdx view,
                          ViewIdx otherView,
                          DimIdx dimension,
                          DimIdx otherDimension)
{

    assert( dimension >= 0 && dimension < getNDimensions() &&
           otherDimension >= 0 && otherDimension < other->getNDimensions() );

    bool ret = false;
    T otherValue = (T)other->getRawValue(otherDimension, otherView);

    QMutexLocker k(&_valueMutex);
    typename PerViewValueMap::const_iterator foundView = _values[dimension].find(view);
    if (foundView == _values[dimension].end()) {
        // View does not exist in this knob, create it
        ret = true;
        _values[dimension].insert(std::make_pair(view, otherValue));
    } else {
        // Copy value
        ret |= foundView->second != otherValue;
        foundView->second = otherValue;
    }

    return ret;
}

template<>
bool
KnobStringBase::cloneValues(const KnobIPtr& other,
                            ViewSetSpec view,
                            ViewSetSpec otherView,
                            DimSpec dimension,
                            DimSpec otherDimension)
{
    if (!other) {
        return false;
    }
    assert((view.isAll() && otherView.isAll()) || (view.isViewIdx() && view.isViewIdx()));
    assert((dimension.isAll() && otherDimension.isAll()) || (!dimension.isAll() && !otherDimension.isAll()));

    KnobStringBasePtr isString = toKnobStringBase(other);
    ///Can only clone strings
    assert(isString);
    bool hasChanged = false;
    if (isString) {
        std::list<ViewIdx> views = other->getViewsList();
        if (dimension.isAll()) {

            int nDims = getNDimensions();
            for (int i = 0; i < nDims; ++i) {
                if (view.isAll()) {
                    for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                        hasChanged |= copyValueForType<std::string>(isString, *it, *it, DimIdx(i), DimIdx(i));
                    }
                } else {
                    hasChanged |= copyValueForType<std::string>(isString, ViewIdx(view), ViewIdx(otherView), DimIdx(i), DimIdx(i));
                }
            }
            
        } else {
            if (view.isAll()) {
                for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                    hasChanged |= copyValueForType<std::string>(isString, *it, *it, DimIdx(dimension), DimIdx(otherDimension));
                }
            } else {
                hasChanged |= copyValueForType<std::string>(isString, ViewIdx(view), ViewIdx(otherView), DimIdx(dimension), DimIdx(otherDimension));
            }
        }
    }
    return hasChanged;
}


template<typename T>
bool
Knob<T>::cloneValues(const KnobIPtr& other,
                     ViewSetSpec view,
                     ViewSetSpec otherView,
                     DimSpec dimension,
                     DimSpec otherDimension)
{
    if (!other) {
        return false;
    }
    assert((view.isAll() && otherView.isAll()) || (view.isViewIdx() && view.isViewIdx()));
    assert((dimension.isAll() && otherDimension.isAll()) || (!dimension.isAll() && !otherDimension.isAll()));

    KnobIntBasePtr isInt = toKnobIntBase(other);
    KnobBoolBasePtr isBool = toKnobBoolBase(other);
    KnobDoubleBasePtr isDouble = toKnobDoubleBase(other);
    assert(isInt || isBool || isDouble);

    bool hasChanged = false;
    if (isInt || isBool || isDouble) {
        std::list<ViewIdx> views = other->getViewsList();
        if (dimension.isAll()) {

            int nDims = getNDimensions();
            for (int i = 0; i < nDims; ++i) {
                if (view.isAll()) {
                    for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                        if (isInt) {
                            hasChanged |= copyValueForType<int>(isInt, *it, *it, DimIdx(i), DimIdx(i));
                        } else if (isBool) {
                            hasChanged |= copyValueForType<bool>(isBool, *it, *it, DimIdx(i), DimIdx(i));
                        } else if (isDouble) {
                            hasChanged |= copyValueForType<double>(isDouble, *it, *it, DimIdx(i), DimIdx(i));
                        }
                    }
                } else {
                    if (isInt) {
                        hasChanged |= copyValueForType<int>(isInt, ViewIdx(view), ViewIdx(otherView), DimIdx(i), DimIdx(i));
                    } else if (isBool) {
                        hasChanged |= copyValueForType<bool>(isBool, ViewIdx(view), ViewIdx(otherView), DimIdx(i), DimIdx(i));
                    } else if (isDouble) {
                        hasChanged |= copyValueForType<double>(isDouble, ViewIdx(view), ViewIdx(otherView), DimIdx(i), DimIdx(i));
                    }
                }
            }

        } else {
            if (view.isAll()) {
                for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                    if (isInt) {
                        hasChanged |= copyValueForType<int>(isInt, *it, *it, DimIdx(dimension), DimIdx(otherDimension));
                    } else if (isBool) {
                        hasChanged |= copyValueForType<bool>(isBool, *it, *it, DimIdx(dimension), DimIdx(otherDimension));
                    } else if (isDouble) {
                        hasChanged |= copyValueForType<double>(isDouble, *it, *it, DimIdx(dimension), DimIdx(otherDimension));
                    }
                }
            } else {
                if (isInt) {
                    hasChanged |= copyValueForType<int>(isInt, ViewIdx(view), ViewIdx(otherView), DimIdx(dimension), DimIdx(otherDimension));
                } else if (isBool) {
                    hasChanged |= copyValueForType<bool>(isBool, ViewIdx(view), ViewIdx(otherView), DimIdx(dimension), DimIdx(otherDimension));
                } else if (isDouble) {
                    hasChanged |= copyValueForType<double>(isDouble, ViewIdx(view), ViewIdx(otherView), DimIdx(dimension), DimIdx(otherDimension));
                }
            }
        }
    }
    return hasChanged;
}

template <typename T>
void
Knob<T>::cloneExpressionsResults(const KnobIPtr& other,
                                 ViewSetSpec view,
                                 ViewSetSpec otherView,
                                 DimSpec dimension,
                                 DimSpec otherDimension)
{
    if (!other) {
        return;
    }
    assert((view.isAll() && otherView.isAll()) || (view.isViewIdx() && view.isViewIdx()));
    assert((dimension.isAll() && otherDimension.isAll()) || (!dimension.isAll() && !otherDimension.isAll()));

    boost::shared_ptr<Knob<T> > otherKnob = boost::dynamic_pointer_cast<Knob<T> >(other);
    if (!otherKnob) {
        return;
    }

    QMutexLocker k(&_valueMutex);
    std::list<ViewIdx> views = other->getViewsList();
    if (dimension.isAll()) {
        int dimMin = std::min( getNDimensions(), other->getNDimensions() );
        for (int i = 0; i < dimMin; ++i) {
            if (view.isAll()) {
                for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                    FrameValueMap results;
                    otherKnob->getExpressionResults(DimIdx(i), *it, results);
                    _exprRes[i][*it] = results;
                }
            } else {
                FrameValueMap results;
                otherKnob->getExpressionResults(DimIdx(i), otherView, results);
                _exprRes[i][view] = results;
            }
        }
    } else {
        if (view.isAll()) {
            for (std::list<ViewIdx>::const_iterator it= views.begin(); it != views.end(); ++it) {
                FrameValueMap results;
                otherKnob->getExpressionResults(otherDimension, *it, results);
                _exprRes[dimension][*it] = results;
            }
        } else {
            FrameValueMap results;
            otherKnob->getExpressionResults(otherDimension, otherView, results);
            _exprRes[dimension][view] = results;
        }

    }
}

template<typename T>
bool
Knob<T>::copyKnob(const KnobIPtr& other,
                  ViewSetSpec view,
                  DimSpec dimension,
                  ViewSetSpec otherView,
                  DimSpec otherDimension,
                  const RangeD* range,
                  double offset)
{
    if (!other || other.get() == this) {
        // Cannot clone itself
        return false;
    }
    beginChanges();

    bool hasChanged = false;
    hasChanged |= cloneValues(other, view, otherView, dimension, otherDimension);
    hasChanged |= cloneExpressions(other, view, otherView, dimension, otherDimension);
    hasChanged |= cloneCurves(other, view, otherView, dimension, otherDimension, offset, range);
    hasChanged |= cloneExtraData(other, view, otherView, dimension, otherDimension, offset, range);

    if ( getHolder() ) {
        getHolder()->updateHasAnimation();
    }
    evaluateValueChange(dimension, getCurrentTime(), view, eValueChangedReasonNatronInternalEdited);
    endChanges();

    return hasChanged;
}


template <typename T>
void
Knob<T>::cloneDefaultValues(const KnobIPtr& other)
{
    int dims = std::min( getNDimensions(), other->getNDimensions() );

    boost::shared_ptr<Knob<T> > otherT = boost::dynamic_pointer_cast<Knob<T> >(other);
    assert(otherT);
    if (!otherT) {
        // coverity[dead_error_line]
        return;
    }

    std::vector<DefaultValue> otherDef;
    {
        QMutexLocker l(&otherT->_valueMutex);
        otherDef = otherT->_defaultValues;
    }
    for (int i = 0; i < dims; ++i) {
        if (otherDef[i].defaultValueSet) {
            setDefaultValueWithoutApplying(otherDef[i].value, i);
        }
    }
}


template <typename T>
bool
Knob<T>::computeValuesHaveModifications(DimIdx /*dimension*/,
                                        const T& value,
                                        const T& defaultValue) const
{
    return value != defaultValue;
}

template <typename T>
void
Knob<T>::computeHasModifications()
{
    bool oneChanged = false;

    int nDims = getNDimensions();
    std::list<ViewIdx> views = getViewsList();

    for (int i = 0; i < nDims; ++i) {
        for (std::list<ViewIdx>::const_iterator it = views.begin(); it != views.end(); ++it) {

            bool hasModif = false;
            std::string expr = getExpression(DimIdx(i), *it);
            if ( !expr.empty() ) {
                hasModif = true;
            }

            if (!hasModif) {
                CurvePtr c = getCurve(*it, DimIdx(i));
                if ( c && c->isAnimated() ) {
                    hasModif = true;
                }
            }

            if (!hasModif) {
                MasterKnobLink linkData;
                if (getMaster(DimIdx(i), *it, &linkData)) {
                    hasModif = true;
                }
            }

            if (!hasModif) {
                QMutexLocker k(&_valueMutex);
                typename PerViewValueMap::const_iterator foundView = _values[i].find(*it);
                if (foundView != _values[i].end()) {
                    if ( computeValuesHaveModifications(DimIdx(i), foundView->second, _defaultValues[i].value) ) {
                        hasModif = true;
                    }
                }
            }


            if (!hasModif) {
                hasModif |= hasModificationsVirtual(DimIdx(i), *it);
            }
            
            oneChanged |= setHasModifications(DimIdx(i), *it, hasModif, true);
        } // for all views

    } // for all dimensions
    if (oneChanged) {
        _signalSlotHandler->s_hasModificationsChanged();
    }
}

template <typename T>
void
Knob<T>::copyValuesFromCurve(DimSpec dim, ViewSetSpec view)
{
    double time = getCurrentTime();
    std::list<ViewIdx> views = getViewsList();
    if (dim.isAll()) {
        int nDims = getNDimensions();
        for (int i = 0; i < nDims; ++i) {
            if (view.isAll()) {

                for (std::list<ViewIdx>::const_iterator it = views.begin(); it != views.end(); ++it) {

                    T v = getValueAtTime(time, DimIdx(i), ViewGetSpec(*it));

                    QMutexLocker l(&_valueMutex);
                    typename PerViewValueMap::const_iterator foundView = _values[i].find(*it);
                    if (foundView == _values[i].end()) {
                        continue;
                    }
                    foundView->second = v;
                }
            } else {
                ViewIdx view_i = getViewIdxFromGetSpec(ViewGetSpec(view.value()));
                T v = getValueAtTime(time, DimIdx(i), ViewGetSpec(view_i));

                QMutexLocker l(&_valueMutex);
                typename PerViewValueMap::const_iterator foundView = _values[i].find(view_i);
                if (foundView == _values[i].end()) {
                    continue;
                }
                foundView->second = v;
            }
        }
    } else {
        if (dim < 0 || dim >= getNDimensions()) {
            throw std::invalid_argument("Knob::copyValuesFromCurve: Dimension out of range");
        }
        if (view.isAll()) {
            for (std::list<ViewIdx>::const_iterator it = views.begin(); it != views.end(); ++it) {
                T v = getValueAtTime(time, dim, ViewGetSpec(*it));

                QMutexLocker l(&_valueMutex);
                typename PerViewValueMap::const_iterator foundView = _values[dim].find(*it);
                if (foundView == _values[dim].end()) {
                    return;
                }
                foundView->second = v;
            }
        } else {
            ViewIdx view_i = getViewIdxFromGetSpec(ViewGetSpec(view.value()));
            T v = getValueAtTime(time, dim, ViewGetSpec(view_i));

            QMutexLocker l(&_valueMutex);
            typename PerViewValueMap::const_iterator foundView = _values[dim].find(view_i);
            if (foundView == _values[dim].end()) {
                return;
            }
            foundView->second = v;
        }
    }
} // copyValuesFromCurve


// All keyframes of the curve for all dimensions are added to the cache.
// Typically this is needed for animated parameters that have an effect overtime which is
// integrated such as a speed param of a retimer: the speed could be 5 at frame 1 and 10 at frame 100
// but if the speed changes to 6 at frame 1, then output at frame 100 should change as well
template <typename T>
void handleAnimatedHashing(Knob<T>* knob, ViewIdx view, DimIdx dimension, Hash64* hash)
{
    CurvePtr curve = knob->getCurve(view, dimension);
    assert(curve);
    Hash64::appendCurve(curve, hash);

}

template <>
void handleAnimatedHashing(Knob<std::string>* knob, ViewIdx view, DimIdx dimension, Hash64* hash)
{
    AnimatingKnobStringHelper* isAnimated = dynamic_cast<AnimatingKnobStringHelper*>(knob);
    assert(isAnimated);
    if (isAnimated) {
        StringAnimationManagerPtr mng = isAnimated->getStringAnimation();
        std::map<ViewIdx,std::map<double, std::string> > keys;
        mng->save(&keys);
        for (std::map<ViewIdx,std::map<double, std::string> >::iterator it = keys.begin(); it!=keys.end(); ++it) {
            for (std::map<double, std::string>::iterator it2 = it->second.begin(); it2!=it->second.end(); ++it2) {
                Hash64::appendQString(QString::fromUtf8(it2->second.c_str()), hash);
            }
        }
    } else {
        CurvePtr curve = knob->getCurve(view, dimension);
        assert(curve);
        Hash64::appendCurve(curve, hash);
    }

}

template <typename T>
void appendValueToHash(const T& v, Hash64* hash)
{
    hash->append(v);
}

template <>
void appendValueToHash(const bool& v, Hash64* hash)
{
    hash->append(v);
}


template <>
void appendValueToHash(const std::string& v, Hash64* hash)
{
    Hash64::appendQString(QString::fromUtf8(v.c_str()), hash);
}

template <typename T>
void
Knob<T>::appendToHash(double time, ViewIdx view, Hash64* hash)
{
    int nDims = getNDimensions();

    KnobFrameViewHashingStrategyEnum hashingStrat = getHashingStrategy();


    for (int i = 0; i < nDims; ++i) {
        if (hashingStrat == eKnobHashingStrategyAnimation && isAnimated(i)) {
            handleAnimatedHashing(this, view, i, hash);
        } else {
            T v = getValueAtTime(time, i, view);
            appendValueToHash(v, hash);
        }
    }
}

template <>
AnimatingObjectI::KeyframeDataTypeEnum
Knob<int>::getKeyFrameDataType() const
{
    return AnimatingObjectI::eKeyframeDataTypeInt;
}

template <>
AnimatingObjectI::KeyframeDataTypeEnum
Knob<bool>::getKeyFrameDataType() const
{
    return AnimatingObjectI::eKeyframeDataTypeBool;
}

template <>
AnimatingObjectI::KeyframeDataTypeEnum
Knob<double>::getKeyFrameDataType() const
{
    return AnimatingObjectI::eKeyframeDataTypeDouble;
}

template <>
AnimatingObjectI::KeyframeDataTypeEnum
Knob<std::string>::getKeyFrameDataType() const
{
    return AnimatingObjectI::eKeyframeDataTypeString;
}

template <typename T>
void
Knob<T>::clearExpressionsResults(DimSpec dimension, ViewSetSpec view)
{
    QMutexLocker k(&_valueMutex);

    if (dimension.isAll()) {
        for (int i = 0; i < getNDimensions(); ++i) {
            const PerViewFrameValueMap& retPerView = _exprRes[i];
            if (view.isAll()) {
                for (typename PerViewFrameValueMap::const_iterator it = _exprRes[i].begin(); it!=_exprRes[i].end(); ++it) {
                    it->second.clear();
                }
            } else {
                ViewIdx view_i = getViewIdxFromGetSpec(ViewGetSpec(view.value()));
                typename PerViewFrameValueMap::const_iterator foundView = _exprRes[i].find(view_i);
                if (foundView == _exprRes[i].end()) {
                    return;
                }
                foundView->second.clear();
            }
        }
    } else {
        if (dimension < 0 || dimension >= getNDimensions()) {
            throw std::invalid_argument("Knob::clearExpressionsResults: Dimension out of range");
        }
        const PerViewFrameValueMap& retPerView = _exprRes[dimension];
        if (view.isAll()) {
            for (typename PerViewFrameValueMap::const_iterator it = _exprRes[dimension].begin(); it!=_exprRes[dimension].end(); ++it) {
                it->second.clear();
            }
        } else {
            ViewIdx view_i = getViewIdxFromGetSpec(ViewGetSpec(view.value()));
            typename PerViewFrameValueMap::const_iterator foundView = _exprRes[dimension].find(view_i);
            if (foundView == _exprRes[dimension].end()) {
                return;
            }
            foundView->second.clear();
        }
    }

}

template <typename T>
void
Knob<T>::getExpressionResults(DimIdx dim, ViewGetSpec view, FrameValueMap& map) const
{
    if (dim < 0 || dim >= getNDimensions()) {
        throw std::invalid_argument("Knob::getExpressionResults: Dimension out of range");
    }

    QMutexLocker k(&_valueMutex);


    ViewIdx view_i = getViewIdxFromGetSpec(view);
    const PerViewFrameValueMap& retPerView = _exprRes[dim];
    typename PerViewFrameValueMap::const_iterator foundView = _exprRes[dim].find(view_i);
    if (foundView == _exprRes[dim].end()) {
        return;
    }
    map = foundView->second;
}


NATRON_NAMESPACE_EXIT;

#endif // KNOBIMPL_H
