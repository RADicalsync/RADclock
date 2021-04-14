#ifndef DAGMETA_UTIL_H
#define DAGMETA_UTIL_H

#include <ifaddrs.h>

/* dag headers */
#include "dag_platform.h"
#include "dagapi.h"
#include "dagutil.h"
#include "erfmeta.h"
#include "dag_config_api.h"

typedef struct {
    uint16_t total_nodes;
    uint64_t phys_memory[65536];
}numa_mem_t;

int dagmeta_parse_file(FILE * parser_in);
int dagmeta_parser_set_log(FILE * parser_out);

int dagmeta_write_record(erfmeta_record_t *record, char *filename, int append);

/** Returns the hostname of the machine by reference.
 * 
 *  @param char array to be filled in.
 *  @return N.A
 **/
int dagmeta_populate_hostname(char g_array[]);

/** Returns the OS information by reference.
 * 
 *  @param char array to be filled in.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_populate_osinfo(char g_array[]);

/** Returns the model information by reference.
 *:TODO - This is currently a dummy function that returns
 * "Probe SKU". Need to add code to return Probe Stock Keeping Unit.
 *  @param char array to be filled in.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_populate_modelnifo(char g_array[]);

/** Returns the OS total physical memory of the system in bytes by reference.
 * 
 *  @param uint64_t * to be filled in.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_populate_memoryinfo(uint64_t *g_uint64);

/** Returns the probe serial number  by reference.
 *:TODO - This is currently a dummy function that returns
 * "12345678". Need to add code to return Probe serial number as string.
 *  @param char array to be filled in.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_populate_serialnumber(char g_array[]);

/** Returns the timezone  by reference.
 *  @param int32_t  to be filled in with current timezone.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_timezone(int32_t *g_int32);

/** Returns the timezone name by reference.
 *  @param g_array[] to be filled in with current timezone name.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_timezone_name(char g_array[]);

/** Returns DAG card serial number string by reference.
 *  @param g_array  to be filled in with card serial number string.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_serial_number(dag_card_ref_t card_ref, dag_component_t card_root, char g_array[]);

/** Returns flow hash mode of the DAG card (n_tuple_select).
 *  @param g_uint32  to be filled in with flow hash bitfield.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_flow_hash_mode(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t* g_uint32);

/** Returns full name of the current active firmware image.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param g_array[] to be filled in with current firmware image name.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_active_firmware_version(dag_card_ref_t card_ref, dag_component_t card_root, char g_array[]);

/** Returns model information of the Optics module of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param iface_id  dag interface index
 *  @param g_array[] to be filled in with current firmware image name.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_optics_model(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t iface_id, char g_array[]);

/** Returns the assigned MAC address of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param interface_id  dag interface index
 *  @param g_array[] to be filled in with current firmware image name.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_mac_address(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, char g_array[]);

/** Returns the link status of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param g_uint8[] to be filled with the link status of the specified interface.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_link_status(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id,uint8_t *g_uint8);

/** Returns the transceiver id if the Optics module of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param g_array[] to be filled in with transceiver id of the optics module.
 *  @return 0 on success
 *          -1 on failure
 **/
int dagmeta_get_card_iface_transceiver_id(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, char g_array[]);

/** Returns the line_rate  of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param g_array[] to be filled with the line_rate of the interface.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_phy_mode(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, char g_array[]);

/** Returns the port type of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param port_type to be filled with the port type of the interface. 1 = Capture Port, 2 = Timing Port.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_port_type(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, uint32_t *port_type);

/** Returns the bitrate  of the specified interface.
 *  @param bitrate to be filled with the bitrate of the interface..
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_speed(char phyrate[], uint64_t *bitrate);

/** Returns the name of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param g_array to be filled with the name of the interface. "Timing Port" for timing ports, else "Port X".
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_name(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, char g_array[]);

/** Returns the interface id number of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param iface_num to be filled with the number of the interface.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_num(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, uint32_t *iface_num);

/** Returns the rx power  of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param rx_power to be filled with the rx_power of the interface in milliBel(1mW).
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_transceiver_rx_power(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, int32_t *rx_power);

/** Returns the tx power of the specified interface.
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param interface_id  dag interface index
 *  @param tx_power to be filled with the tx_power of the interface in milliBel(1mW).
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_transceiver_tx_power(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, int32_t *tx_power);


/** Returns the stream memory in bytes 
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param stream_num  dag stream index
 *  @param meminfo to be filled with the stream memory in bytes.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_stream_meminfo(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t stream_num, uint64_t *meminfo);

/** Returns the stream snap length in bytes 
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param stream_num  dag stream index
 *  @param snaplen to be filled with the stream snap length in bytes.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_stream_snaplen(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t stream_num, uint32_t *snaplen);

/** Returns the stream drop count in records
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param stream_num  dag stream index
 *  @param drop to be filled with the stream drop in records
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_stream_drop(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t stream_num, uint32_t *drop);

/** Returns the stream AF drop count in records
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param stream_num  dag stream index
 *  @param buf_drop to be filled with the stream AF drop in records
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_stream_buf_drop(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t stream_num, uint32_t *buf_drop);

/** Returns the card tunnelling mode bitfield
 *  @param card_ref  dag_config card reference
 *  @param card_root dag_config card root component
 *  @param tunnelling_mode to be filled with the mode bitfield.
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_tunneling_mode(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t * tunneling_mode);

/** Returns the IPv4 or IPv6 address from dagnet device 
 *  @param card_ref  dag_config card reference
 *  @param interface_id  dag interface index
 *  @param ipv6_flag enable return of IPv6 addresses
 *  @param ifaddrs struct to be filled in with IP address
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_ipv4_ipv6_addr(dag_card_ref_t card_ref, dag_component_t card_root, uint32_t interface_id, int ipv6_flag, struct ifaddrs **addr);

/** Returns the IPv4 or IPv6 cidr mask from dagnet device 
 *  @param card_ref  dag_config card reference
 *  @param addr  address to get mask for
 *  @param g_uint32 to be filled in with cidr mask
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_card_iface_ipv4_ipv6_cidr(dag_card_ref_t card_ref, struct ifaddrs *addr, uint32_t * g_uint32);

/** Returns the current system time in ERF format 
 *  @param g_uint64  current system time in ERF format
 *  @return 0 on success -1 on failure
 **/
int dagmeta_populate_gentime(uint64_t * g_uint64);

/** Generates a host_id (in host byte order) using the MAC address of a given interface.
 *  @param ifname host interface name
 *  @param host_id generated host id (in host byte order)
 *  @return 0 on success -1 on failure
 **/
int dagmeta_get_hostid_from_iface(char * ifname, uint64_t *host_id);


int dagmeta_populate_modelinfo(char g_array[]);
int dagmeta_populate_cpuinfo(char g_array[]);
int dagmeta_populate_cpucores(uint32_t * g_uint32);
int dagmeta_populate_numa_nodes(uint32_t * g_uint32);
int dagmeta_get_card_model(dag_card_ref_t card_ref, char g_array[]);

uint8_t dagmeta_parser_get_source_id();
uint64_t dagmeta_parser_get_host_id();
uint8_t dagmeta_parser_get_color();

#endif //DAGMETA_UTIL_H
