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

#ifndef Engine_RotoItem_h
#define Engine_RotoItem_h

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "Global/Macros.h"

#include <list>
#include <set>
#include <string>

#if !defined(Q_MOC_RUN) && !defined(SBK_RUN)
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#endif

CLANG_DIAG_OFF(deprecated-declarations)
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMetaType>
CLANG_DIAG_ON(deprecated-declarations)

#include "Global/GlobalDefines.h"
#include "Engine/FitCurve.h"
#include "Engine/EngineFwd.h"
#include "Engine/KnobItemsTable.h"
#include "Serialization/SerializationBase.h"


#define kRotoLayerBaseName "Layer"
#define kRotoBezierBaseName "Bezier"
#define kRotoOpenBezierBaseName "Pencil"
#define kRotoEllipseBaseName "Ellipse"
#define kRotoRectangleBaseName "Rectangle"
#define kRotoPaintBrushBaseName "Brush"
#define kRotoPaintEraserBaseName "Eraser"
#define kRotoPaintBlurBaseName "Blur"
#define kRotoPaintSmearBaseName "Smear"
#define kRotoPaintSharpenBaseName "Sharpen"
#define kRotoPaintCloneBaseName "Clone"
#define kRotoPaintRevealBaseName "Reveal"
#define kRotoPaintDodgeBaseName "Dodge"
#define kRotoPaintBurnBaseName "Burn"

NATRON_NAMESPACE_ENTER;

struct RotoItemPrivate;
class RotoItem
    : public KnobTableItem
{
public:

    enum SelectionReasonEnum
    {
        eSelectionReasonOverlayInteract = 0, ///when the user presses an interact
        eSelectionReasonSettingsPanel, ///when the user interacts with the settings panel
        eSelectionReasonOther ///when the project loader restores the selection
    };

public:

    // This class is virtual pure so no need to privatize the constructor

    RotoItem(const KnobItemsTablePtr& model);


    virtual ~RotoItem();

    virtual bool isItemContainer() const OVERRIDE { return false; }


    ///only callable from the main-thread
    void setGloballyActivated(bool a, bool setChildren);

    ///MT-safe
    bool isGloballyActivated() const;

    bool isDeactivatedRecursive() const;

    void setLocked(bool l, bool lockChildren, RotoItem::SelectionReasonEnum reason);
    bool getLocked() const;

    bool isLockedRecursive() const;

    
protected:

    virtual void initializeKnobs() OVERRIDE;

private:

    void setGloballyActivated_recursive(bool a);
    void setLocked_recursive(bool locked, RotoItem::SelectionReasonEnum reason);

    boost::scoped_ptr<RotoItemPrivate> _imp;
};

inline RotoItemPtr
toRotoItem(const KnobHolderPtr& holder)
{
    return boost::dynamic_pointer_cast<RotoItem>(holder);
}

inline RotoItemConstPtr
toRotoItem(const KnobHolderConstPtr& holder)
{
    return boost::dynamic_pointer_cast<const RotoItem>(holder);
}

NATRON_NAMESPACE_EXIT;

Q_DECLARE_METATYPE(NATRON_NAMESPACE::RotoItem*);

#endif // Engine_RotoItem_h
