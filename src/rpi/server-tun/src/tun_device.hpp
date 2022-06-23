#ifndef ITS_SERVER_TUN_SRC_TUN_DEVICE_HPP_
#define ITS_SERVER_TUN_SRC_TUN_DEVICE_HPP_


#include <string>


class tun_device
{
public:
	tun_device();
	tun_device(std::string name);
	tun_device(const tun_device & other) = delete;
	tun_device & operator=(const tun_device & other) = delete;
	tun_device(tun_device && other);
	tun_device & operator=(tun_device && other);
	~tun_device();

	void swap(tun_device & other);

	void open(std::string name);
	bool is_open() const;
	void close();

	void set_ip(const std::string & ip, int netmask);
	void set_up(bool up);
	void set_mtu(int mtu);

	size_t read_packet(uint8_t * buffer, size_t buffer_size);
	size_t write_packet(const uint8_t * buffer, size_t buffer_size);

	int fd() { return _fd; }
	const std::string & name() const { return _name; }

private:
	std::string _name;
	int _fd;
};


inline void swap(tun_device & left, tun_device & right) { left.swap(right); }


#endif /* ITS_SERVER_TUN_SRC_TUN_DEVICE_HPP_ */
