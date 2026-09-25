//
// rommanager.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#include <circle/logger.h>
#include <fatfs/ff.h>

#include "rommanager.h"

LOGMODULE("rommanager");
const char* const Disks[] = { "SD", "USB" };
const char ROMDirectory[] = "roms";

// Custom File class for mt32emu
class CROMFile : public MT32Emu::AbstractFile
{
public:
	CROMFile() : m_File{}, m_pData(nullptr) {}

	virtual ~CROMFile() override { close(); }

	virtual size_t getSize() override { return f_size(&m_File); }

	virtual const MT32Emu::Bit8u* getData() override { return m_pData; }

	virtual bool open(const char* pFileName)
	{
		FRESULT Result = f_open(&m_File, pFileName, FA_READ);
		if (Result != FR_OK)
			return false;

		FSIZE_t nSize = f_size(&m_File);
		if (nSize > MaxROMFileSize)
			return false;

		if (!(m_pData = new MT32Emu::Bit8u[nSize]))
			return false;

		UINT nRead;
		Result = f_read(&m_File, m_pData, nSize, &nRead);

		return Result == FR_OK;
	}

	virtual void close() override
	{
		f_close(&m_File);
		if (m_pData)
		{
			delete[] m_pData;
			m_pData = nullptr;
		}
	}

private:
	// The largest ROM is the CM-32L PCM ROM at 1MB; files larger than this cannot be valid
	static constexpr size_t MaxROMFileSize = 1 * MEGABYTE;

	FIL m_File;
	MT32Emu::Bit8u* m_pData;
};

CROMManager::CROMManager()
	: m_pMT32OldControl(nullptr),
	  m_pMT32NewControl(nullptr),
	  m_pCM32LControl(nullptr),

	  m_pNukedMT32Control{nullptr},

	  m_pMT32PCM(nullptr),
	  m_pCM32LPCM(nullptr)
{
}

CROMManager::~CROMManager()
{
	for (
		size_t i = 0;
		i < NukedMT32ROMVersionCount;
		++i
	)
	{
		const MT32Emu::ROMImage* const pROM =
			m_pNukedMT32Control[i];

		if (
			pROM &&
			pROM != m_pMT32OldControl &&
			pROM != m_pMT32NewControl
		)
		{
			if (MT32Emu::File* const pFile = pROM->getFile())
				delete pFile;

			MT32Emu::ROMImage::freeROMImage(pROM);
		}
	}

	const MT32Emu::ROMImage** const ROMs[] =
	{
		&m_pMT32OldControl,
		&m_pMT32NewControl,
		&m_pCM32LControl,
		&m_pMT32PCM,
		&m_pCM32LPCM
	};

	for (const MT32Emu::ROMImage** pROMPtr : ROMs)
	{
		if (!*pROMPtr)
			continue;

		if (MT32Emu::File* const pFile = (*pROMPtr)->getFile())
			delete pFile;

		MT32Emu::ROMImage::freeROMImage(*pROMPtr);
	}
}

bool CROMManager::ScanROMs()
{
	DIR Dir;
	FILINFO FileInfo;
	FRESULT Result;
	CString DirectoryPath;

	// Loop over each disk
	for (auto pDisk : Disks)
	{
		DirectoryPath.Format("%s:/%s", pDisk, ROMDirectory);
		Result = f_findfirst(&Dir, &FileInfo, DirectoryPath, "*");

		// Loop over each file in the directory
		while (Result == FR_OK && *FileInfo.fname)
		{
			// Ensure not directory, hidden, or system file
			if (!(FileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
			{
				// Assemble path
				CString ROMPath(static_cast<const char*>(DirectoryPath));
				ROMPath.Append("/");
				ROMPath.Append(FileInfo.fname);

				// Try to open file
				CheckROM(ROMPath);

			}

			Result = f_findnext(&Dir, &FileInfo);
		}
	}

	return HaveROMSet(TMT32ROMSet::Any);
}

bool CROMManager::HaveROMSet(TMT32ROMSet ROMSet) const
{
	switch (ROMSet)
	{
		case TMT32ROMSet::Any:
			return ((m_pMT32OldControl || m_pMT32NewControl) && m_pMT32PCM) || (m_pCM32LControl && m_pCM32LPCM);

		case TMT32ROMSet::All:
			return m_pMT32OldControl && m_pMT32NewControl && m_pCM32LControl && m_pMT32PCM && m_pCM32LPCM;

		case TMT32ROMSet::MT32Old:
			return m_pMT32OldControl && m_pMT32PCM;

		case TMT32ROMSet::MT32New:
			return m_pMT32NewControl && m_pMT32PCM;

		case TMT32ROMSet::CM32L:
			return m_pCM32LControl && m_pCM32LPCM;
	}

	return false;
}

bool CROMManager::GetROMSet(TMT32ROMSet ROMSet, TMT32ROMSet& pOutROMSet, const MT32Emu::ROMImage*& pOutControl, const MT32Emu::ROMImage*& pOutPCM) const
{
	if (!HaveROMSet(ROMSet))
		return false;

	switch (ROMSet)
	{
		case TMT32ROMSet::Any:
			if (m_pMT32OldControl)
			{
				pOutControl = m_pMT32OldControl;
				pOutROMSet  = TMT32ROMSet::MT32Old;
			}
			else if (m_pMT32NewControl)
			{
				pOutControl = m_pMT32NewControl;
				pOutROMSet  = TMT32ROMSet::MT32New;
			}
			else
			{
				pOutControl = m_pCM32LControl;
				pOutROMSet  = TMT32ROMSet::CM32L;
			}

			if (pOutControl == m_pCM32LControl)
				pOutPCM = m_pCM32LPCM;
			else
				pOutPCM = m_pMT32PCM;

			break;

		case TMT32ROMSet::MT32Old:
			pOutControl = m_pMT32OldControl;
			pOutPCM     = m_pMT32PCM;
			pOutROMSet  = TMT32ROMSet::MT32Old;
			break;

		case TMT32ROMSet::MT32New:
			pOutControl = m_pMT32NewControl;
			pOutPCM     = m_pMT32PCM;
			pOutROMSet  = TMT32ROMSet::MT32New;
			break;

		case TMT32ROMSet::CM32L:
			pOutControl = m_pCM32LControl;
			pOutPCM     = m_pCM32LPCM;
			pOutROMSet  = TMT32ROMSet::CM32L;
			break;

		default:
			return false;
	}

	return true;
}

bool CROMManager::GetNukedMT32ROMSet(
	TNukedMT32ROMVersion Version,
	const MT32Emu::ROMImage*& pOutControl,
	const MT32Emu::ROMImage*& pOutPCM
) const
{
	const size_t nIndex =
		static_cast<size_t>(Version);

	if (nIndex >= NukedMT32ROMVersionCount)
		return false;

	if (
		!m_pNukedMT32Control[nIndex] ||
		!m_pMT32PCM
	)
	{
		return false;
	}

	pOutControl =
		m_pNukedMT32Control[nIndex];

	pOutPCM =
		m_pMT32PCM;

	return true;
}

bool CROMManager::CheckROM(const char* pPath)
{
	CROMFile* pFile = new CROMFile();
	if (!pFile->open(pPath))
	{
		LOGERR("Couldn't open '%s' for reading", pPath);
		delete pFile;
		return false;
	}

	// Check ROM and store if valid
	const MT32Emu::ROMImage* pROM = MT32Emu::ROMImage::makeROMImage(pFile);
	if (!StoreROM(*pROM))
	{
		MT32Emu::ROMImage::freeROMImage(pROM);
		delete pFile;
		return false;
	}

	return true;
}

bool CROMManager::StoreROM(
	const MT32Emu::ROMImage& ROMImage
)
{
	const MT32Emu::ROMInfo* const pROMInfo =
		ROMImage.getROMInfo();

	if (!pROMInfo)
		return false;

	if (
		pROMInfo->type ==
		MT32Emu::ROMInfo::Type::Control
	)
	{
		struct TExactROM
		{
			const char* pShortName;
			size_t nIndex;
		};

		static const TExactROM ExactROMs[] =
		{
			{"ctrl_mt32_1_04", 0},
			{"ctrl_mt32_1_05", 1},
			{"ctrl_mt32_1_06", 2},
			{"ctrl_mt32_1_07", 3},
			{"ctrl_mt32_2_04", 4},
			{"ctrl_mt32_2_06", 5},
			{"ctrl_mt32_2_07", 6}
		};

		bool bStoredExact = false;

		for (const auto& ExactROM : ExactROMs)
		{
			if (
				strcmp(
					pROMInfo->shortName,
					ExactROM.pShortName
				) != 0
			)
			{
				continue;
			}

			if (
				m_pNukedMT32Control[
					ExactROM.nIndex
				]
			)
			{
				return false;
			}

			m_pNukedMT32Control[
				ExactROM.nIndex
			] = &ROMImage;

			bStoredExact = true;
			break;
		}

		const MT32Emu::ROMImage** pFamily = nullptr;

		if (
			pROMInfo->shortName[10] == '1' ||
			pROMInfo->shortName[10] == 'b'
		)
		{
			pFamily = &m_pMT32OldControl;
		}
		else if (
			pROMInfo->shortName[10] == '2'
		)
		{
			pFamily = &m_pMT32NewControl;
		}
		else
		{
			pFamily = &m_pCM32LControl;
		}

		if (!*pFamily)
		{
			*pFamily = &ROMImage;
			return true;
		}

		return bStoredExact;
	}

	if (
		pROMInfo->type ==
		MT32Emu::ROMInfo::Type::PCM
	)
	{
		const MT32Emu::ROMImage** pPCM =
			pROMInfo->shortName[4] == 'm'
				? &m_pMT32PCM
				: &m_pCM32LPCM;

		if (*pPCM)
			return false;

		*pPCM = &ROMImage;
		return true;
	}

	return false;
}
