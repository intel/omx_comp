//#define LOG_NDEBUG 0

#define LOG_TAG "mrst_psb"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <OMX_Core.h>

#include "avc.h"

AvcCodecDataConstructor::AvcCodecDataConstructor() {
	is_done = false;
	m_buffer = NULL;
	m_length = 0;
}

AvcCodecDataConstructor::~AvcCodecDataConstructor() {
	if (m_buffer) {
		delete[] m_buffer;
	}
}

bool AvcCodecDataConstructor::add_buffer(unsigned char *buffer,
		unsigned int length) {
	if (is_done) {
		return true;
	}

	if (!buffer || !length) {
		return false;
	}

	unsigned int nal_unit_type = 0x1F & (*buffer);
	if (nal_unit_type != 0x07 && nal_unit_type != 0x08) {
		return false;
	}

	if (!m_buffer) {
		m_buffer = new unsigned char[length];
		if (!m_buffer) {
			return false;
		}
		memcpy(m_buffer, buffer, length);
		m_length = length;
		return true;
	}

	unsigned int nal_unit_type_prv = 0x1F & (*m_buffer);
	if (nal_unit_type_prv == nal_unit_type) {
		return false;
	}

	unsigned char *tbuf = new unsigned char[m_length + length + 8 + 3];
	if (!tbuf) {
		return false;
	}

	unsigned char *sps_nal = m_buffer;
	unsigned int sps_nal_length = m_length;

	unsigned char *pps_nal = buffer;
	unsigned int pps_nal_length = length;

	if (nal_unit_type_prv == 0x08) {
		sps_nal = buffer;
		sps_nal_length = length;
		pps_nal = m_buffer;
		pps_nal_length = m_length;
	}

	unsigned char len[2];

	tbuf[0] = 0x00;
	tbuf[1] = 0x00;
	tbuf[2] = 0x00;
	tbuf[3] = 0x00;
	tbuf[4] = 0x03;
	tbuf[5] = 0x01;

	*((unsigned short *) len) = sps_nal_length;
	tbuf[6] = len[1];
	tbuf[7] = len[0];
	memcpy(tbuf + 8, sps_nal, sps_nal_length);

	LOGV("--- add_buffer: tbuf[6] = 0x%x tbuf[7] = 0x%x --\n", tbuf[6], tbuf[7]);

	*(tbuf + 8 + sps_nal_length) = 0x01;
	*((unsigned short *) len) = pps_nal_length;
	*(tbuf + 8 + sps_nal_length + 1) = len[1];
	*(tbuf + 8 + sps_nal_length + 2) = len[0];
	memcpy((tbuf + 8 + sps_nal_length + 3), pps_nal, pps_nal_length);

	delete[] m_buffer;
	m_buffer = tbuf;
	m_length = m_length + length + 8 + 3;

	is_done = true;
	return true;
}

unsigned char *AvcCodecDataConstructor::get_avc_codec_data(unsigned int &length) {
	length = 0;
	if (!is_done) {
		return NULL;
	}

        LOGV("************ codec data *************\n");
        for(int idx = 0; idx < m_length; idx++) {
            LOGV("buffer[%d] = 0x%x\n", idx, m_buffer[idx]);
        }
        LOGV("************ codec data *************\n");

	length = m_length;
	return m_buffer;
}

AvcFrameNals::AvcFrameNals() {
	nal_count = 0;
}

AvcFrameNals::~AvcFrameNals() {
}

bool AvcFrameNals::add_nal(unsigned char *buffer, unsigned int offset,
		unsigned int length, void *appdata) {
	if (nal_count > 256) {
		return false;
	}

	nals[nal_count].buffer = buffer;
	nals[nal_count].offset = offset;
	nals[nal_count].appdata = appdata;
	nals[nal_count++].length = length;

	return true;
}

NalBuffer* AvcFrameNals::get_nals(unsigned int &nal_count) {

	nal_count = this->nal_count;
	if (nal_count == 0) {
		return NULL;
	}
	return nals;
}

void AvcFrameNals::reset() {
	nal_count = 0;
}

