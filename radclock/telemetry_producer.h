/*
 * Copyright (C) 2023 The RADclock Project (see AUTHORS file)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ring_buffer.h"


#define TELEMETRY_PRODUCER_LOG_VERBOSITY 1
#define TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL 1
#define TRIGGER_CA_ERR_THRESHOLD 1e-3 // 1 ms

void
update_prior_data(struct radclock_handle *handle)
{
	if (handle->conf->time_server_ntc_count > handle->telemetry_data.prior_data_size)
	{
		struct Ring_Buffer_Prior_Data * new_data = (struct Ring_Buffer_Prior_Data *)
		    malloc( sizeof(struct Ring_Buffer_Prior_Data) * handle->conf->time_server_ntc_count);
		// Copy old data into larger data structure
		memcpy(new_data, handle->telemetry_data.prior_data,
		    sizeof(struct Ring_Buffer_Prior_Data) * handle->telemetry_data.prior_data_size);
		// Zero the data in the data structures
		memset(&new_data[handle->telemetry_data.prior_data_size+1],
		    sizeof(struct Ring_Buffer_Prior_Data) * (handle->telemetry_data.prior_data_size - handle->conf->time_server_ntc_count), 0);

		free(handle->telemetry_data.prior_data);

		// Set the newly allocated data last message time to current time.
		// Allows some time for the stats on the clocks to get set instead of immediately timing out
		// vcounter_t vcount;
		// radclock_get_vcounter(handle->clock, &vcount);
		// long double radclock_ts;
		// read_RADabs_UTC(&handle->rad_data[handle->pref_sID], &vcount, &radclock_ts, 0);
		// for (int i = handle->telemetry_data.prior_data_size; i < handle->conf->time_server_ntc_count; i++)
		//     new_data[i].last_msg_time = radclock_ts;

		handle->telemetry_data.prior_data_size = handle->conf->time_server_ntc_count;

		handle->telemetry_data.prior_data = new_data;
	}
}


/* For non-NTC servers, changes in the corresponding RADclock are not used as triggers */
int
active_trigger(struct radclock_handle *handle, long double *radclock_ts, int sID, int PICN, int is_sID_NTC, uint64_t SA)
{
	// Check if the number of clocks has increased
	update_prior_data(handle);

	// If PICN has changed
	if (handle->telemetry_data.prior_PICN != PICN)
		return 1;

	// If public serving status changed
	if (handle->telemetry_data.prior_public_ntp != handle->conf->public_ntp ) {
		verbose(LOG_NOTICE, "Telemetry Producer - public serving state changed");
		return 1;
	}

	if (is_sID_NTC)		// if an NTC server
	{
		// If clock status word has changed
		if (handle->telemetry_data.prior_data[sID].prior_status != handle->rad_data[sID].status) {
			verbose(VERB_DEBUG, "Telemetry Producer - status changed");
			return 1;
		}

		// If clock underlying asymmetry has changed
		// struct bidir_algostate *state = &((struct bidir_peers *)handle->peers)->state[sID];   // old name
		struct bidir_algostate *state = &((struct bidir_algodata*)handle->algodata)->state[sID];
		if (handle->telemetry_data.prior_data[sID].prior_uA != state->Asymhat) {
			verbose(VERB_DEBUG, "Telemetry Producer - Asymmetry has changed");
			return 1;
		}

		// If clock error bound over threshold
		if (handle->rad_data[sID].ca_err > TRIGGER_CA_ERR_THRESHOLD) {
			verbose(VERB_DEBUG, "Telemetry Producer - Error bound exceeded");
			return 1;
		}

		// If clock has just passed a leap second
		if (handle->rad_data[sID].leapsec_total != handle->telemetry_data.prior_data[sID].prior_leapsec_total) {
			verbose(LOG_NOTICE, "Telemetry Producer - leap seconds have changed %d");
			return 1;
		}

		// If servertrust word has changed  TODO: check this, why not check entire word?
		if ((handle->telemetry_data.prior_data[sID].prior_servertrust & (1ULL << sID)) != (handle->servertrust & (1ULL << sID)) ) {
			verbose(VERB_DEBUG, "Telemetry Producer - ServerTrust status changed");
			return 1;
		}

		// If SA status word has changed
		if (handle->telemetry_data.prior_data[sID].prior_sa != SA) {
			verbose(LOG_NOTICE, "Telemetry Producer - Server Anomaly (SA) changed");
			return 1;
		}

	}

	vcounter_t vcount;
	radclock_get_vcounter(handle->clock, &vcount);
	read_RADabs_UTC(&handle->rad_data[handle->pref_sID], &vcount, radclock_ts, 0);

	if (*radclock_ts - handle->telemetry_data.last_msg_time >= TELEMETRY_KEEP_ALIVE_SECONDS) {
		verbose(VERB_DEBUG, "Telemetry Producer - Synchronous keep alive trigger %d seconds since last message",
		    (int)(*radclock_ts - handle->telemetry_data.last_msg_time));
		return 1;
	}

	return 0;
}


int
push_telemetry(struct radclock_handle *handle, int sID)
{
	// If telemetry is disabled return
	if (handle->conf->server_telemetry != BOOL_ON)
		return 0;

	/* Check if we need to send a keep alive message */
	long double radclock_ts = 0;
	int keep_alive_trigger = 0;
	int is_sID_NTC = 0;
	int PICN = handle->conf->time_server_ntc_mapping[handle->pref_sID]; // -1 if not an NTC node

	if (sID > -1 && handle->conf->time_server_ntc_mapping[sID] != -1)
		is_sID_NTC = 1;

	/* ntc_status holds flag-form summary of SAs across active NTC servers */
	uint64_t SA = 0;
	if (handle->conf->is_cn)
		SA = ((struct bidir_perfdata *)handle->perfdata)->ntc_status;

	if (active_trigger(handle, &radclock_ts, sID, PICN, is_sID_NTC, SA))
	{
		// If the telemetry data changed then use the time of the last packet
		// Otherwise use the current time as its a keep alive message - set flag keep_alive_trigger
		// TODO: should use PICN I think, and use macros
		if (radclock_ts == 0)
			read_RADabs_UTC(&handle->rad_data[sID], &handle->rad_data[sID].last_changed,  &radclock_ts, 0);
		else
			keep_alive_trigger = 1;

		push_telemetry_batch(
			handle->telemetry_data.packetId, &handle->telemetry_data.write_pos,
			handle->telemetry_data.buffer,   &handle->telemetry_data.holding_buffer_size,
			handle->telemetry_data.holding_buffer, handle, radclock_ts,
			keep_alive_trigger, PICN, handle->nservers, SA);

		handle->telemetry_data.packetId += 1;
		handle->telemetry_data.last_msg_time = radclock_ts;
		handle->telemetry_data.prior_PICN = PICN;
		handle->telemetry_data.prior_public_ntp = handle->conf->public_ntp; // update to current value

		if (is_sID_NTC) {
			struct bidir_algostate *state = &((struct bidir_algodata*)handle->algodata)->state[sID];
			handle->telemetry_data.prior_data[sID].prior_status = handle->rad_data[sID].status;
			handle->telemetry_data.prior_data[sID].prior_uA = state->Asymhat;;
			handle->telemetry_data.prior_data[sID].prior_leapsec_total = handle->rad_data[sID].leapsec_total;
			handle->telemetry_data.prior_data[sID].prior_minRTT = handle->ntp_server[sID].minRTT;

			handle->telemetry_data.prior_data[sID].prior_sa = SA;
			handle->telemetry_data.prior_data[sID].prior_servertrust = handle->servertrust;
		}

	}

	return (0);
}


int
push_telemetry_batch(int packetId, int *ring_write_pos, void * shared_memory_handle,
    int *holding_buffer_size, void* holding_buffer, struct radclock_handle *handle,
    long double timestamp, int keep_alive_trigger,  int PICN, int NTC_Count, uint64_t SA)
{
	int bytes_written = 0;

	// If the packet is larger than the holding buffer then make a new larger holding buffer
	int packet_size = sizeof(Radclock_Telemetry_Latest)
	    + sizeof(Radclock_Telemetry_NTC_Latest) * NTC_Count
	    + sizeof(Radclock_Telemetry_Footer);
	check_and_grow_holding_buffer(holding_buffer_size, packet_size, holding_buffer);

	if (packet_size > RADCLOCK_RING_BASE_BUFFER_BYTES) {
		verbose(LOG_ERR, "Telemetry Producer Error packet too large %d vs max_size:%d",
		    packet_size, RADCLOCK_RING_BASE_BUFFER_BYTES);
		return RADCLOCK_ERROR_DATA_TOO_LARGE;
	}
		int start_write_buffer_pos = *ring_write_pos;

		// Create a telemetry OCN specific packet
		Radclock_Telemetry_Latest * header_data = (Radclock_Telemetry_Latest *) holding_buffer;

   // if (keep_alive_trigger)
    //{
    //    // If a keep alive trigger assume all the data the same. But change header data
   //     header_data->header.packetId = packetId;
  //      header_data->timestamp = timestamp;
 //   }
//    else
		{
			int delta_accepted_public_ntp = handle->accepted_public_ntp - handle->prior_ntp_sent;
			int delta_rejected_public_ntp = handle->rejected_public_ntp - handle->prior_ntp_rejected;
//			verbose(LOG_NOTICE, "Telem delta %d %d  Acc(%d, %d), Rej(%d, %d)\n",
//					 delta_accepted_public_ntp,
//					 delta_rejected_public_ntp,
//					 handle->accepted_public_ntp, handle->prior_ntp_sent,
//					 handle->rejected_public_ntp, handle->prior_ntp_rejected);

			handle->prior_ntp_sent     = handle->accepted_public_ntp;
			handle->prior_ntp_rejected = handle->rejected_public_ntp;

			// UGLY hack to test grafana code:
//			delta_rejected_public_ntp = 100;

			// printf("Telemetry SA val: %d\n", SA);
			*header_data = make_telemetry_packet(packetId, PICN, NTC_Count, timestamp,
			    delta_accepted_public_ntp, delta_rejected_public_ntp,
			    handle->servertrust, handle->conf->public_ntp, SA);

			// Write OCN specific telemetry packet to shared memory
			// bytes_written += ring_buffer_write(&header_data, sizeof(Radclock_Telemetry_Latest), shared_memory_handle, ring_write_pos);
			for (int s=0; s < NTC_Count; s++) {
				int sID = handle->conf->time_server_ntc_indexes[s];
				unsigned int status_word = handle->rad_data[sID].status;
				int NTC_id = handle->conf->time_server_ntc_mapping[sID];

				struct bidir_algostate *state = &((struct bidir_algodata*)handle->algodata)->state[sID];
				double uA = state->Asymhat; // (float)(rand()%1000) * 0.001;
				double err_bound = handle->rad_data[sID].ca_err;
				double min_RTT   = handle->rad_error[sID].min_RTT; //;handle->ntp_server[sID].minRTT;
				double clockErr = 0;
				if (handle->conf->is_cn)
					clockErr= ((struct bidir_perfdata *)(handle->perfdata))->state[sID].RADerror;

				// Create a telemetry NTC server specific packet
				Radclock_Telemetry_NTC_Latest * NTC_data = (Radclock_Telemetry_NTC_Latest *)
				    (holding_buffer + sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_NTC_Latest)*s);
				*NTC_data = make_telemetry_NTC_packet(status_word, NTC_id, uA, err_bound, min_RTT, clockErr);
			}

			// Create and copy footer data to holding buffer
			Radclock_Telemetry_Footer * footer_data = (Radclock_Telemetry_Footer *)
			    (holding_buffer + sizeof(Radclock_Telemetry_Latest) +
			    sizeof(Radclock_Telemetry_NTC_Latest)*NTC_Count);
			*footer_data = make_telemetry_footer(packet_size);

			if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 1)
				verbose(LOG_NOTICE, "Telemetry Producer Footer: wp:%d footer_size:%d",
				    *ring_write_pos, footer_data->size);
		}

		// Push all data into the ring buffer in a single batch write
		bytes_written += ring_buffer_write(holding_buffer, packet_size, shared_memory_handle, ring_write_pos);

		// Update the ring head positions
		flush_heads(shared_memory_handle, start_write_buffer_pos, *ring_write_pos);

		if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 0 &&
		    header_data->header.packetId % TELEMETRY_PRODUCER_LOG_PACKET_INTERVAL == 0) // && header_data.header.packetId % 1000 == 0
			verbose(VERB_DEBUG, "Telemetry Producer: start ts:%.09Lf wp:%d current wp:%d: v:%d NTCs:%d size:%d id:%d",
			    timestamp, start_write_buffer_pos, *ring_write_pos, header_data->header.version,
			    header_data->NTC_Count, header_data->header.size, header_data->header.packetId);

		if (TELEMETRY_PRODUCER_LOG_VERBOSITY > 2 )
			for (int s = 0; s < NTC_Count; s++) {
				int offset = sizeof(Radclock_Telemetry_Latest) + sizeof(Radclock_Telemetry_NTC_Latest)*s;
				Radclock_Telemetry_NTC_Latest * NTC_data = (Radclock_Telemetry_NTC_Latest *) (holding_buffer + offset);

				verbose(VERB_DEBUG, "Telemetry Producer NTC: wp:%d: stat:%u id:%d uA:%le err:%le (%d, %d)",
				    *ring_write_pos, NTC_data->status_word, NTC_data->NTC_id, NTC_data->uA,
				    NTC_data->err_bound, offset, *holding_buffer_size);
			}

		if (bytes_written != header_data->header.size)
			verbose(LOG_ERR, "Telemetry Producer Error packet bytes: %d different from packet.header.size: %d",
			    bytes_written, header_data->header.size);

		return bytes_written;
}
