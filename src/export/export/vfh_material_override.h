//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
#define VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H

#include "vfh_geoutils.h"

#include <QString>

#include <hash_map.h>

#include <GA/GA_Handle.h>
#include <OBJ/OBJ_Node.h>
#include <STY/STY_Styler.h>

namespace VRayForHoudini {

struct MtlOverrideItem {
	enum MtlOverrideItemType {
		itemTypeNone = 0,
		itemTypeInt,
		itemTypeDouble,
		itemTypeVector,
		itemTypeString,
	};

	MtlOverrideItem()
		: type(itemTypeNone)
		, valueInt(0)
		, valueDouble(0.0)
		, valueVector(0.0f, 0.0f, 0.0f)
	{}

	/// Sets override value type.
	void setType(MtlOverrideItemType value) { type = value; }

	/// Returns override value type.
	MtlOverrideItemType getType() const { return type; }

	/// Override value type.
	MtlOverrideItemType type;

	exint valueInt;
	fpreal valueDouble;
	VRay::Vector valueVector;
	QString valueString;
};

typedef VUtils::HashMap<MtlOverrideItem> MtlOverrideItems;

struct PrimMaterial {
	PrimMaterial()
		: matNode(nullptr)
	{}

	/// Merge overrides overwriting existing values.
	void merge(const PrimMaterial &other);

	/// Merge overrides overwriting existing values.
	void mergeOverrides(const MtlOverrideItems &items);

	/// Merge overrides not overwriting existing values.
	void append(const PrimMaterial &other);

	/// Merge overrides not overwriting existing values.
	void appendOverrides(const MtlOverrideItems &items);

	/// Material node (SHOP, VOP).
	OP_Node *matNode;

	/// Material overrides from stylesheet or SHOP overrides.
	MtlOverrideItems overrides;
};

struct MtlOverrideAttrExporter {
	explicit MtlOverrideAttrExporter(const GA_Detail &gdp) {
		buildAttributesList(gdp, GA_ATTRIB_PRIMITIVE, primAttrList);
		buildAttributesList(gdp, GA_ATTRIB_POINT,     pointAttrList);
	}

	void fromPrimitive(MtlOverrideItems &overrides, GA_Offset offs) const {
		addAttributesAsOverrides(primAttrList, offs, overrides);
	}

	void fromPoint(MtlOverrideItems &overrides, GA_Offset offs) const {
		addAttributesAsOverrides(pointAttrList, offs, overrides);
	}

	static void buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList);

private:
	static void addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides);

	GEOAttribList primAttrList;
	GEOAttribList pointAttrList;
};

void mergeStyleSheet(PrimMaterial &primMaterial,
					 const QString &styleSheet,
					 fpreal t,
					 int materialOnly=false);

void mergeMaterialOverrides(PrimMaterial &primMaterial,
							const UT_String &matPath,
							const UT_String &materialOverrides,
							fpreal t,
							int materialOnly=false);

void mergeMaterialOverride(PrimMaterial &primMaterial,
						   const GA_ROHandleS &materialStyleSheetHndl,
						   const GA_ROHandleS &materialPathHndl,
						   const GA_ROHandleS &materialOverrideHndl,
						   GA_Offset primOffset,
						   fpreal t);

/// Get styler for the object.
/// @param objNode OBJ node instance.
/// @param t Time.
STY_Styler getStylerForObject(OBJ_Node &objNode, fpreal t);

/// Get styler for the primitive.
/// @param geoStyler Current top level styler.
/// @param prim Primitive instance.
STY_Styler getStylerForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim);

/// Fills style sheet material overrides for a primitive.
/// @param geoStyler Current top level styler.
/// @param prim Primitive instance.
/// @param primMaterial Material to set value to.
void getOverridesForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim, PrimMaterial &primMaterial);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
