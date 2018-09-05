/**
*  Copyright (C) 2018 3D Repo Ltd
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU Affero General Public License as
*  published by the Free Software Foundation, either version 3 of the
*  License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Affero General Public License for more details.
*
*  You should have received a copy of the GNU Affero General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <OdaCommon.h>

#include <StaticRxObject.h>
#include <RxInit.h>
#include <RxDynamicModule.h>
#include <DynamicLinker.h>
#include <DgDatabase.h>
#include <RxDynamicModule.h>

#include <ExSystemServices.h>
#include <ExDgnServices.h>
#include <ExDgnHostAppServices.h>

#include <DgGiContext.h>
#include <DgGsManager.h>

#include "oda_file_processor.h"
#include "oda_gi_dumper.h"
#include "oda_vectorise_device.h"

#include <DgLine.h>      // This file puts OdDgLine3d in the output file
using namespace repo::manipulator::modelconvertor::odaHelper;

OdaFileProcessor::OdaFileProcessor(const std::string &inputFile, OdaGeometryCollector *geoCollector)
	: file(inputFile),
	  collector(geoCollector)
{
}


OdaFileProcessor::~OdaFileProcessor()
{
}

OdResult importDGN(OdDbBaseDatabase *pDb, const ODCOLORREF* pPallete, int numColors) {
	OdResult ret = eOk;
	OdUInt32 iCurEntData = 0;
	try
	{
		odgsInitialize();

		ret = CollectData(pDb, &m_entityDataArr, &m_pColladaMaterialData, &exportLights, pPallete, numColors, pEntity);
	}
	catch (...)
	{
		ret = eExtendedError;
	}
	return ret;
}

}

int OdaFileProcessor::readFile() {

	int   nRes = 0;               // Return value for the function
	OdStaticRxObject<RepoDgnServices> svcs;
	OdString fileSource = file.c_str();
	OdString strStlFilename = OdString::kEmpty;


	/**********************************************************************/
	/* Initialize Runtime Extension environment                           */
	/**********************************************************************/
	odrxInitialize(&svcs);
	odgsInitialize();


	try
	{
		/**********************************************************************/
		/* Initialize Teigha™ for .dgn files                                               */
		/**********************************************************************/
		::odrxDynamicLinker()->loadModule(L"TG_Db", false);

		OdDgDatabasePtr pDb = svcs.readFile(fileSource);

		if (!pDb.isNull())
		{
			const ODCOLORREF* refColors = OdDgColorTable::currentPalette(pDb);
			ODGSPALETTE pPalCpy;
			pPalCpy.insert(pPalCpy.begin(), refColors, refColors + 256);


			OdDgElementId elementId = pDb->getActiveModelId();
			if (elementId.isNull())
			{
				elementId = pDb->getDefaultModelId();
				pDb->setActiveModelId(elementId);
			}
			OdDgElementId elementActId = pDb->getActiveModelId();
			OdDgModelPtr pModel = elementId.safeOpenObject();
			ODCOLORREF background = pModel->getBackground();

			OdDgElementId vectorizedViewId;
			OdDgViewGroupPtr pViewGroup = pDb->getActiveViewGroupId().openObject();
			if (pViewGroup.isNull())
			{
				//  Some files can have invalid id for View Group. Try to get & use a valid (recommended) View Group object.
				pViewGroup = pDb->recommendActiveViewGroupId().openObject();
				if (pViewGroup.isNull())
				{
					// Add View group
					pModel->createViewGroup();
					pModel->fitToView();
					pViewGroup = pDb->recommendActiveViewGroupId().openObject();
				}
			}
			if (!pViewGroup.isNull())
			{
				OdDgElementIteratorPtr pIt = pViewGroup->createIterator();
				for (; !pIt->done(); pIt->step())
				{
					OdDgViewPtr pView = OdDgView::cast(pIt->item().openObject());
					if (pView.get() && pView->getVisibleFlag())
					{
						vectorizedViewId = pIt->item();
						break;
					}
				}
			}

			if (vectorizedViewId.isNull() && !pViewGroup.isNull())
			{
				OdDgElementIteratorPtr pIt = pViewGroup->createIterator();

				if (!pIt->done())
				{
					OdDgViewPtr pView = OdDgView::cast(pIt->item().openObject(OdDg::kForWrite));

					if (pView.get())
					{
						pView->setVisibleFlag(true);
						vectorizedViewId = pIt->item();
					}
				}
			}

			if (vectorizedViewId.isNull())
			{
				repoError << "Can not find an active view group or all its views are disabled";
			}
			else
			{
				// Color with #255 always defines backround. The background of the active model must be considered in the device palette.
				pPalCpy[255] = background;
				// Note: This method should be called to resolve "white background issue" before setting device palette
				bool bCorrected = OdDgColorTable::correctPaletteForWhiteBackground(pPalCpy.asArrayPtr());

				OdResult res = importDGN(pDb, pPalCpy.asArrayPtr(), 256);
				if (eOk == res)
				{
					repoError << "Successfully exported.";
				}
				else
				{
					OdString tmp = OD_T("Error : ") + OdError(res).description();
					repoError << tmp.c_str();
				}
			}
		}

		pDb = 0;
	}
	catch (OdError& e)
	{
		//FIXME: use repo log
		std::cerr << (L"\nTeigha(R) for .DGN Error: %ls\n", e.description().c_str());
	}
	catch (...)
	{
		/*STD(cout) << "\n\nUnexpected error.";*/
		nRes = -1;
		throw;
	}

	/**********************************************************************/
	/* Uninitialize Runtime Extension environment                         */
	/**********************************************************************/

	odgsUninitialize();
	::odrxUninitialize();

	return nRes;
}
