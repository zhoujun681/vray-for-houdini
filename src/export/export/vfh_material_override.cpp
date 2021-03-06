//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_material_override.h"
#include "vfh_prm_templates.h"
#include "vfh_log.h"
#include "vfh_attr_utils.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <GA/GA_AttributeFilter.h>
#include <GEO/GEO_Detail.h>

#include <STY/STY_StylerGroup.h>
#include <STY/STY_TargetMatchStatus.h>
#include <STY/STY_OverrideValues.h>
#include <STY/STY_OverrideValuesFilter.h>
#include <STY/STY_Subject.h>
#include <GSTY/GSTY_SubjectPrim.h>
#include <GSTY/GSTY_SubjectPrimGroup.h>

#include <rapidjson/document.h>

#include <QStringList>
#include <QRegExp>

using namespace VRayForHoudini;
using namespace rapidjson;

namespace Styles {
	const char styles[] = "styles";

	const char flags[] = "flags";

	namespace Flags {
		const char mute[] = "mute";
		const char solo[] = "solo";
	}

	const char target[] = "target";

	namespace Target {
		const char subTarget[] = "subTarget";
		const char group[] = "group";
	}

	const char overrides[] = "overrides";

	namespace Overrides {
		const char material[] = "material";
		const char materialParameters[] = "materialParameters";

		namespace Material {
			const char name[] = "name";
		}
	}
}

void PrimMaterial::merge(const PrimMaterial &other)
{
	if (other.matNode) {
		matNode = other.matNode;
	}
	mergeOverrides(other.overrides);
}

void PrimMaterial::mergeOverrides(const MtlOverrideItems &items)
{
	FOR_CONST_IT (MtlOverrideItems, it, items) {
		overrides.insert(it.key(), it.data());
	}
}

void PrimMaterial::append(const PrimMaterial &other)
{
	if (!matNode && other.matNode) {
		matNode = other.matNode;
	}
	appendOverrides(other.overrides);
}

void PrimMaterial::appendOverrides(const MtlOverrideItems &items)
{
	FOR_CONST_IT (MtlOverrideItems, it, items) {
		if (overrides.find(it.key()) == overrides.end()) {
			overrides.insert(it.key(), it.data());
		}
	}
}

static void parseStyleSheetOverrides(const Value &paramOverrides, MtlOverrideItems &overrides)
{
	for (Value::ConstMemberIterator pIt = paramOverrides.MemberBegin(); pIt != paramOverrides.MemberEnd(); ++pIt) {
		const char *parmName = pIt->name.GetString();
		const Value &paramJsonValue = pIt->value;

		MtlOverrideItems::iterator moIt = overrides.find(parmName);
		if (moIt != overrides.end()) {
			// There is already some top-level override for this parameter.
			continue;
		}

		// NOTES:
		//  * Assuming current array value is a color
		//  * Only first 3 components will be used for vectors
		if (paramJsonValue.IsArray()) {
			if (paramJsonValue.Size() >= 3) {
				MtlOverrideItem &overrideItem = overrides[parmName];
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector.set(paramJsonValue[0].GetDouble(),
												paramJsonValue[1].GetDouble(),
												paramJsonValue[2].GetDouble());
			}
			else {
				// TODO: Implement other cases / print warning.
			}
		}
		else if (paramJsonValue.IsDouble()) {
			MtlOverrideItem &overrideItem = overrides[parmName];
			overrideItem.setType(MtlOverrideItem::itemTypeDouble);
			overrideItem.valueDouble = paramJsonValue.GetDouble();
		}
		else if (paramJsonValue.IsString()) {
			// TODO: String type.
		}
		else {
			// TODO: Implement other cases / print warning.
		}
	}
}

void VRayForHoudini::mergeStyleSheet(PrimMaterial &primMaterial, const QString &styleSheet, fpreal t, int materialOnly)
{
	Document document;
	document.Parse(styleSheet.toLocal8Bit().constData());

	if (!document.HasMember(Styles::styles))
		return;

	const Value &styles = document[Styles::styles];
	UT_ASSERT(styles.IsArray());

	for (Value::ConstValueIterator it = styles.Begin(); it != styles.End(); ++it) {
		const Value &style = *it;
		if (style.IsObject() && style.HasMember(Styles::overrides)) {
			const Value &styleOver = style[Styles::overrides];

			if (styleOver.HasMember(Styles::Overrides::material)) {
				const Value &mtlOver = styleOver[Styles::Overrides::material];
				if (mtlOver.HasMember(Styles::Overrides::Material::name)) {
					const char *matPath = mtlOver[Styles::Overrides::Material::name].GetString();

					OP_Node *matNode = getOpNodeFromPath(matPath, t);
					if (matNode) {
						// There is already some top-level material assigned.
						if (!primMaterial.matNode) {
							primMaterial.matNode = matNode;
						}
					}

					if (materialOnly) {
						break;
					}
				}
			}

			if (!materialOnly && styleOver.HasMember(Styles::Overrides::materialParameters)) {
				const Value &paramOverrides = styleOver[Styles::Overrides::materialParameters];

				parseStyleSheetOverrides(paramOverrides, primMaterial.overrides);
			}
		}
	}
}

void VRayForHoudini::mergeMaterialOverrides(PrimMaterial &primMaterial, const UT_String &matPath, const UT_String &materialOverrides, fpreal t, int materialOnly)
{
	// There is already some top-level material assigned.
	if (!primMaterial.matNode) {
		primMaterial.matNode = getOpNodeFromPath(matPath, t);
	}

	if (primMaterial.matNode && !materialOnly) { 
		//
		// { "diffuser" : 1.0, "diffuseg" : 1.0, "diffuseb" : 1.0 }
		//
		SHOP_GeoOverride mtlOverride;
		if (mtlOverride.load(materialOverrides.buffer())) {
			UT_StringArray mtlOverrideKeys;
			mtlOverride.getKeys(mtlOverrideKeys);

			UT_StringArray validKeys;

			// Check keys if there is already some top-level override for this parameter.
			for (const UT_StringHolder &key : mtlOverrideKeys) {
				int channelIIdx = -1;
				PRM_Parm *keyParm = primMaterial.matNode->getParmList()->getParmPtrFromChannel(key, &channelIIdx);
				if (keyParm && channelIIdx >= 0 && channelIIdx < 4) {
					const char *parmName = keyParm->getToken();

					MtlOverrideItems::iterator moIt = primMaterial.overrides.find(parmName);
					if (moIt == primMaterial.overrides.end()) {
						validKeys.append(key);
					}
				}
			}

			for (const UT_StringHolder &key : validKeys) {
				// Channel for vector components.
				// NOTE: Only first 3 components will be used for vectors.
				int channelIIdx = -1;

				PRM_Parm *keyParm = primMaterial.matNode->getParmList()->getParmPtrFromChannel(key, &channelIIdx);
				if (keyParm && channelIIdx >= 0 && channelIIdx < 4) {
					const PRM_Type &keyParmType = keyParm->getType();
					const char *parmName = keyParm->getToken();

					if (keyParmType.isFloatType()) {
						MtlOverrideItem &overrideItem = primMaterial.overrides[parmName];

						fpreal channelValue = 0.0;
						mtlOverride.import(key, channelValue);

						if (keyParmType.getFloatType() == PRM_Type::PRM_FLOAT_RGBA) {
							overrideItem.setType(MtlOverrideItem::itemTypeVector);
							overrideItem.valueVector[channelIIdx] = channelValue;
						}
						else if (channelIIdx == 0) {
							overrideItem.setType(MtlOverrideItem::itemTypeDouble);
							overrideItem.valueDouble = channelValue;
						}
						else {
							// TODO: Implement other cases / print warning.
						}
					}
					else if (keyParmType.isOrdinalType() && channelIIdx == 0) {
						MtlOverrideItem &overrideItem = primMaterial.overrides[keyParm->getToken()];
						overrideItem.setType(MtlOverrideItem::itemTypeInt);
						mtlOverride.import(key, overrideItem.valueInt);
					}
					else if (keyParmType.isStringType()) {
						// TODO: String type.
					}
					else {
						// TODO: Implement other cases / print warning.
					}
				}
			}
		}
	}
}

void VRayForHoudini::mergeMaterialOverride(PrimMaterial &primMaterial,
										   const GA_ROHandleS &materialStyleSheetHndl,
										   const GA_ROHandleS &materialPathHndl,
										   const GA_ROHandleS &materialOverrideHndl,
										   GA_Offset primOffset,
										   fpreal t)
{
	if (materialStyleSheetHndl.isValid()) {
		const QString &styleSheet = materialStyleSheetHndl.get(primOffset);
		if (!styleSheet.isEmpty()) {
			mergeStyleSheet(primMaterial, styleSheet, t);
		}
	}
	else if (materialPathHndl.isValid()) {
		const UT_String &matPath = materialPathHndl.get(primOffset);

		UT_String materialOverrides;
		if (materialOverrideHndl.isValid()) {
			materialOverrides = materialOverrideHndl.get(primOffset);
		}

		if (!matPath.equal("")) {
			mergeMaterialOverrides(primMaterial, matPath, materialOverrides, t);
		}
	}
}

void MtlOverrideAttrExporter::buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList)
{
	gdp.getAttributes().matchAttributes(
		GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectFloatTuple(false),
			                            GA_AttributeFilter::selectByTupleRange(3,4)),
		owner, attrList);
	gdp.getAttributes().matchAttributes(GA_AttributeFilter::selectAlphaNum(), owner, attrList);
}

void MtlOverrideAttrExporter::addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides)
{
	for (const GA_Attribute *attr : attrList) {
		if (!attr)
			continue;

		const char *attrName = attr->getName().buffer();

		MtlOverrideItems::iterator moIt = overrides.find(attrName);
		if (moIt == overrides.end()) {
			MtlOverrideItem &overrideItem = overrides[attrName];

			GA_ROHandleV3 v3Hndl(attr);
			GA_ROHandleV4 v4Hndl(attr);
			GA_ROHandleS sHndl(attr);
			GA_ROHandleF fHndl(attr);

			if (v4Hndl.isValid()) {
				const UT_Vector4F &c = v4Hndl.get(offs);
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector = utVectorVRayVector(c);
			}
			else if (v3Hndl.isValid()) {
				const UT_Vector3F &c = v3Hndl.get(offs);
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector = utVectorVRayVector(c);
			}
			else if (sHndl.isValid()) {
				overrideItem.setType(MtlOverrideItem::itemTypeString);
				overrideItem.valueString = sHndl.get(offs);
			}
			else if (fHndl.isValid()) {
				overrideItem.setType(MtlOverrideItem::itemTypeDouble);
				overrideItem.valueDouble = fHndl.get(offs);
			}
		}
	}
}

static void mergeOverrideValues(const STY_OverrideValues &styOverrideValues, PrimMaterial &primMaterial)
{
#if UT_MAJOR_VERSION_INT >= 16
	UT_Int64Array intVals;
	UT_Fpreal64Array floatVals;
	UT_StringArray stringVals;

	if (!styOverrideValues.size())
		return;

	for (const auto &styOverrideValueCategory : styOverrideValues) {
		const UT_StringHolder &key = styOverrideValueCategory.first;
		const STY_OverrideValueMap &values = styOverrideValueCategory.second;

		if (key.equal(Styles::Overrides::material)) {
			const auto &nameIt = values.find(Styles::Overrides::Material::name);
			if (nameIt != values.end()) {
				const auto &nameValuePair = *nameIt;

				STY_OptionEntryHandle nameOpt = nameValuePair.second.myValue;
				if (nameOpt->importOption(stringVals)) {
					primMaterial.matNode = getOpNodeFromPath(stringVals[0].buffer());
				}
			}
		}
		else if (key.equal(Styles::Overrides::materialParameters)) {
			for (const auto &value : values) {
				const UT_StringHolder &attrName = value.first;
				STY_OptionEntryHandle opt = value.second.myValue;

				MtlOverrideItems::iterator moIt = primMaterial.overrides.find(attrName);
				if (moIt != primMaterial.overrides.end())
					continue;

				MtlOverrideItem overrideItem;

				if (opt->importOption(intVals)) {
					if (intVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeInt);
						overrideItem.valueInt = intVals(0);
					}
					else if (intVals.size() == 3 ||
							 intVals.size() == 4)
					{
						overrideItem.setType(MtlOverrideItem::itemTypeVector);
						overrideItem.valueVector = utVectorVRayVector(intVals);
					}
				}
				else if (opt->importOption(stringVals)) {
					if (stringVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeString);
						overrideItem.valueString = stringVals(0).buffer();
					}
				}
				else if (opt->importOption(floatVals)) {
					if (floatVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeDouble);
						overrideItem.valueDouble = floatVals(0);
					}
					else if (floatVals.size() == 3 ||
							 floatVals.size() == 4)
					{
						overrideItem.setType(MtlOverrideItem::itemTypeVector);
						overrideItem.valueVector = utVectorVRayVector(floatVals);
					}
				}

				if (overrideItem.getType() != MtlOverrideItem::itemTypeNone) {
					primMaterial.overrides.insert(attrName.buffer(), overrideItem);
				}
			}
		}
	}
#endif
}

static void mergeOverrideValues(const STY_Styler &styler, PrimMaterial &primMaterial)
{
#if UT_MAJOR_VERSION_INT >= 16
	static const STY_OverrideValuesFilter styOverrideValuesFilter(nullptr);

	STY_OverrideValues styOverrideValues;
	styler.getOverrides(styOverrideValues, styOverrideValuesFilter);

	mergeOverrideValues(styOverrideValues, primMaterial);
#endif
}

void VRayForHoudini::getOverridesForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim, PrimMaterial &primMaterial)
{
#if UT_MAJOR_VERSION_INT >= 16
	STY_Styler primStyler = getStylerForPrimitive(geoStyler, prim);
	mergeOverrideValues(primStyler, primMaterial);
#endif
}

STY_Styler VRayForHoudini::getStylerForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim)
{
#if UT_MAJOR_VERSION_INT < 16
	return STY_Styler();
#else
	GSTY_SubjectPrim primSubject(&prim);
	return geoStyler.cloneWithSubject(primSubject);
#endif
}

STY_Styler VRayForHoudini::getStylerForObject(OBJ_Node &objNode, fpreal t)
{
#if UT_MAJOR_VERSION_INT < 16
	return STY_Styler();
#else
	if (!Parm::isParmExist(objNode, VFH_ATTR_SHOP_MATERIAL_STYLESHEET))
		return STY_Styler();

	UT_String styleSheet;
	objNode.evalString(styleSheet, VFH_ATTR_SHOP_MATERIAL_STYLESHEET, 0, t);

	const char *styleBuf = styleSheet.buffer();
	if (!UTisstring(styleBuf))
		return STY_Styler();

	Log::getLog().debug("getStylerForObject(%s)", objNode.getName().buffer()); 

	STY_StyleSheetHandle styStyleSheetHandle(new STY_StyleSheet(styleBuf, NULL, STY_LOAD_FOR_STYLING));
	STY_Styler styStyler(styStyleSheetHandle);

	return styStyler;
#endif
}
