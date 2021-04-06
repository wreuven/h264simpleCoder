/*
 * CJOCh264bitstream.cpp
 *
 *  Created on: Aug 23, 2014
 *      Author: Jordi Cenzano (www.jordicenzano.name)
 */

#define DEFINE_HERE
#include "CJOCh264bitstream.h"

void CJOCh264bitstream_Create(FILE *pOutBinaryFile)
{
	clearbuffer();

	m_pOutFile = pOutBinaryFile;
}

void CJOCh264bitstream_Destroy()
{
	close();
}

void clearbuffer()
{
	memset (&m_buffer,0,sizeof (unsigned char)* BUFFER_SIZE_BITS);
	m_nLastbitinbuffer = 0;
	m_nStartingbyte = 0;
}

int getbitnum (unsigned long lval, int nNumbit)
{
	int lrc = 0;

	unsigned long lmask = (unsigned long) pow((unsigned long)2,(unsigned long)nNumbit);
	if ((lval & lmask) > 0)
		lrc = 1;

	return lrc;
}

void addbittostream (int nVal)
{
	if (m_nLastbitinbuffer >= BUFFER_SIZE_BITS)
	{
		//Must be aligned, no need to do dobytealign();
		savebufferbyte();
	}

	//Use circular buffer of BUFFER_SIZE_BYTES
	int nBytePos = (m_nStartingbyte + (m_nLastbitinbuffer / 8)) % BUFFER_SIZE_BYTES;
	//The first bit to add is on the left
	int nBitPosInByte = 7 - m_nLastbitinbuffer % 8;

	//Get the byte value from buffer
	int nValTmp = m_buffer[nBytePos];

	//Change the bit
	if (nVal > 0)
		nValTmp = (nValTmp | (int) pow(2,nBitPosInByte));
	else
		nValTmp = (nValTmp & ~((int) pow(2,nBitPosInByte)));

	//Save the new byte value to the buffer
	m_buffer[nBytePos] = (unsigned char) nValTmp;

	m_nLastbitinbuffer++;
}

void addbytetostream (int nVal)
{
	if (m_nLastbitinbuffer >= BUFFER_SIZE_BITS)
	{
		//Must be aligned, no need to do dobytealign();
		savebufferbyte();
	}

	//Used circular buffer of BUFFER_SIZE_BYTES
	int nBytePos = (m_nStartingbyte + (m_nLastbitinbuffer / 8)) % BUFFER_SIZE_BYTES;
	//The first bit to add is on the left
	int nBitPosInByte = 7 - m_nLastbitinbuffer % 8;

	//Check if it is byte aligned
	if (nBitPosInByte != 7)
		assert( 0 && "Error: inserting not aligment byte");

	//Add all byte to buffer
	m_buffer[nBytePos] = (unsigned char) nVal;

	m_nLastbitinbuffer = m_nLastbitinbuffer + 8;
}

void dobytealign()
{
	//Check if the last bit in buffer is multiple of 8
	int nr = m_nLastbitinbuffer % 8;
	if ((nr % 8) != 0)
		m_nLastbitinbuffer = m_nLastbitinbuffer + (8 - nr);
}

void savebufferbyte()
{
	int bemulationpreventionexecuted = 0;

	if (m_pOutFile == NULL)
		assert(0 && "Error: out file is NULL");

	//Check if the last bit in buffer is multiple of 8
	if ((m_nLastbitinbuffer % 8)  != 0)
		assert (0 && "Error: Save to file must be byte aligned");

	if ((m_nLastbitinbuffer / 8) <= 0)
		assert ( 0 && "Error: NO bytes to save");

	if (1)
	{
		//Emulation prevention will be used:
		/*As per h.264 spec,
		rbsp_data shouldn't contain
				- 0x 00 00 00
				- 0x 00 00 01
				- 0x 00 00 02
				- 0x 00 00 03

		rbsp_data shall be in the following way
				- 0x 00 00 03 00
				- 0x 00 00 03 01
				- 0x 00 00 03 02
				- 0x 00 00 03 03
		*/

		//Check if emulation prevention is needed (emulation prevention is byte align defined)
		if ((m_buffer[((m_nStartingbyte + 0) % BUFFER_SIZE_BYTES)] == 0x00)&&(m_buffer[((m_nStartingbyte + 1) % BUFFER_SIZE_BYTES)] == 0x00)&&((m_buffer[((m_nStartingbyte + 2) % BUFFER_SIZE_BYTES)] == 0x00)||(m_buffer[((m_nStartingbyte + 2) % BUFFER_SIZE_BYTES)] == 0x01)||(m_buffer[((m_nStartingbyte + 2) % BUFFER_SIZE_BYTES)] == 0x02)||(m_buffer[((m_nStartingbyte + 2) % BUFFER_SIZE_BYTES)] == 0x03)))
		{
			int nbuffersaved = 0;
			unsigned char cEmulationPreventionByte = H264_EMULATION_PREVENTION_BYTE;

			//Save 1st byte
			fwrite(&m_buffer[((m_nStartingbyte + nbuffersaved) % BUFFER_SIZE_BYTES)], 1, 1, m_pOutFile);
			nbuffersaved ++;

			//Save 2st byte
			fwrite(&m_buffer[((m_nStartingbyte + nbuffersaved) % BUFFER_SIZE_BYTES)], 1, 1, m_pOutFile);
			nbuffersaved ++;

			//Save emulation prevention byte
			fwrite(&cEmulationPreventionByte, 1, 1, m_pOutFile);

			//Save the rest of bytes (usually 1)
			while (nbuffersaved < BUFFER_SIZE_BYTES)
			{
				fwrite(&m_buffer[((m_nStartingbyte + nbuffersaved) % BUFFER_SIZE_BYTES)], 1, 1, m_pOutFile);
				nbuffersaved++;
			}

			//All bytes in buffer are saved, so clear the buffer
			clearbuffer();

			bemulationpreventionexecuted = 1;
		}
	}

	if (bemulationpreventionexecuted == 0)
	{
		//No emulation prevention was used

		//Save the oldest byte in buffer
		fwrite(&m_buffer[m_nStartingbyte], 1, 1, m_pOutFile);

		//Move the index
		m_buffer[m_nStartingbyte] = 0;
		m_nStartingbyte++;
		m_nStartingbyte = m_nStartingbyte % BUFFER_SIZE_BYTES;
		m_nLastbitinbuffer = m_nLastbitinbuffer - 8;
	}
}

//Public functions

void addbits (unsigned long lval, int nNumbits)
{
	if ((nNumbits <= 0 )||(nNumbits > 64))
		assert ( 0 && "Error: numbits must be between 1 ... 64");

	int nBit = 0;
	int n = nNumbits-1;
	while (n >= 0)
	{
		nBit = getbitnum (lval,n);
		n--;

		addbittostream (nBit);
	}
}

void addbyte (unsigned char cByte)
{
	//Byte alignment optimization
	if ((m_nLastbitinbuffer % 8)  == 0)
	{
		addbytetostream (cByte);
	}
	else
	{
		addbits (cByte, 8);
	}
}

void addexpgolombunsigned (unsigned long lval)
{
	//it implements unsigned exp golomb coding

	unsigned long lvalint = lval + 1;
	int nnumbits = log2 (lvalint) + 1;

	for (int n = 0; n < (nnumbits-1); n++)
		addbits(0,1);

	addbits(lvalint,nnumbits);
}

void addexpgolombsigned (long lval)
{
	//it implements a signed exp golomb coding

	unsigned long lvalint = abs(lval) * 2 - 1;
	if (lval <= 0)
		lvalint =  2 * abs(lval);

	addexpgolombunsigned(lvalint);
}

void add4bytesnoemulationprevention (unsigned int nVal)
{
	//Used to add NAL header stream
	//Remember: NAL header is byte oriented


	if ((m_nLastbitinbuffer % 8) != 0)
		assert ( 0 && "Error: Save to file must be byte aligned");

	while (m_nLastbitinbuffer != 0)
		savebufferbyte();

	unsigned char cbyte = (nVal & 0xFF000000)>>24;
	fwrite(&cbyte, 1, 1, m_pOutFile);

	cbyte = (nVal & 0x00FF0000)>>16;
	fwrite(&cbyte, 1, 1, m_pOutFile);

	cbyte = (nVal & 0x0000FF00)>>8;
	fwrite(&cbyte, 1, 1, m_pOutFile);

	cbyte = (nVal & 0x000000FF);
	fwrite(&cbyte, 1, 1, m_pOutFile);
}

void close()
{
	//Flush the data in stream buffer

	dobytealign();

	while (m_nLastbitinbuffer != 0)
		savebufferbyte();
}
