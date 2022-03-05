#include "tun_device.hpp"

#include <algorithm>
#include <system_error>
#include <memory>
#include <cstring>
#include <sstream>
#include <cassert>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>


#include "log.hpp"


struct fd_sentry
{
	fd_sentry(int fd_): fd(fd_) {}
	~fd_sentry(){ if (fd < 0) return; ::close(fd); fd = -1; }
	int release() { int rv = fd; fd = -1; return rv; }
	int fd;
};


tun_device::tun_device()
	: _name(), _fd(-1)
{

}


tun_device::tun_device(std::string name)
	: _name(), _fd(-1)
{
	open(std::move(name));
}


tun_device::tun_device(tun_device && other)
	: _name(std::move(other._name)), _fd(std::move(other._fd))
{
	other._name.clear();
	other._fd = -1;
}


tun_device & tun_device::operator=(tun_device && other)
{
	tun_device tmp(std::move(other));
	this->swap(tmp);
	return *this;
}


tun_device::~tun_device()
{
	close();
}


void tun_device::swap(tun_device & other)
{
	std::swap(this->_name, other._name);
	std::swap(this->_fd, other._fd);
}


void tun_device::open(std::string name)
{
	if (name.size() >= IFNAMSIZ) // >= для терминатора в конце
	{
		std::stringstream stream;
		stream << "tun dev name is too big. " << IFNAMSIZ << " is maximum allowed size";
		throw std::invalid_argument(stream.str());
	}

	VLOG_S(INFO) << "opening TUN device \"" << name << "\"";

	// Добираемся до clonedev
	int clone_dev_fd = ::open("/dev/net/tun", O_RDWR);
	if (clone_dev_fd < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to open /dev/net/tun");

	fd_sentry sentry(clone_dev_fd);

	// Создаем себе интерфейс
	struct ifreq ifr;
	std::memset(&ifr, 0x00, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN;
	assert(name.size() < IFNAMSIZ);
	std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);

	int rc = ::ioctl(clone_dev_fd, TUNSETIFF, reinterpret_cast<void*>(&ifr));
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to ioctl tun clone dev");

	_fd = sentry.release();
	_name = ifr.ifr_name;

	VLOG_S(INFO) << "TUN device opened as \"" << _name << "\"";
	// Готово!
}


bool tun_device::is_open() const
{
	return _fd >= 0;
}


void tun_device::close()
{
	if (_fd < 0)
		return;

	VLOG_S(DEBUG) << "closing TUN device \"" << _name << "\"";

	::close(_fd);
	_fd = -1;
	_name.clear();
}


void tun_device::set_ip(const std::string & ip, int mask_size)
{
	if (!is_open())
		throw std::runtime_error("device is not open");

	VLOG_S(INFO) << "setting if \"" << _name << "\" IP to " << ip << "/" << mask_size;

	int socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to open socket for addr setup");

	fd_sentry sentry(socket_fd);

	struct ifreq ifr;

	std::memset(&ifr, 0x00, sizeof(ifr));
	std::strncpy(ifr.ifr_name, _name.c_str(), IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	struct sockaddr_in * addr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
	int rc = ::inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "wrong IP address format");

	rc = ::ioctl(socket_fd, SIOCSIFADDR, &ifr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to set interface IP address");

	uint32_t mask = 0;
	for (int i = 0; i < mask_size; i++)
	{
		mask = mask << 1;
		mask = mask | 1;
	}

	addr->sin_addr.s_addr = mask;
	rc = ::ioctl(socket_fd, SIOCSIFNETMASK, &ifr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to set interface IP mask");

	// Готово!
}


void tun_device::set_up(bool up)
{
	if (!is_open())
		throw std::runtime_error("device is not open");

	VLOG_S(INFO) << "setting if \"" << _name << "\" " << (up ? "up" : "down");

	int socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to open socket for addr setup");

	fd_sentry sentry(socket_fd);

	struct ifreq ifr;
	std::memset(&ifr, 0x00, sizeof(ifr));
	std::strncpy(ifr.ifr_name, _name.c_str(), IFNAMSIZ);

	int rc = ::ioctl(socket_fd, SIOCGIFFLAGS, &ifr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to get if flags");

	if (up)
		ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	else
		ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);

	rc = ::ioctl(socket_fd, SIOCSIFFLAGS, &ifr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to set if flags");
}


void tun_device::set_mtu(int mtu)
{
	if (!is_open())
		throw std::runtime_error("device is not open");

	VLOG_S(INFO) << "setting if \"" << _name << "\" MTU to " << mtu;

	int socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to open socket for addr setup");

	fd_sentry sentry(socket_fd);

	struct ifreq ifr;
	std::memset(&ifr, 0x00, sizeof(ifr));
	std::strncpy(ifr.ifr_name, _name.c_str(), IFNAMSIZ);

	ifr.ifr_mtu = mtu;
	int rc = ::ioctl(socket_fd, SIOCSIFMTU, &ifr);
	if (rc < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to set iterface mtu");

	// готовоу
}


size_t tun_device::read_packet(uint8_t * buffer, size_t buffer_size)
{
	if (!is_open())
		throw std::runtime_error("device is not open");

	int portion = ::read(_fd, buffer, buffer_size);
	if (portion < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to read from tun device");

	return portion;
}


size_t tun_device::write_packet(const uint8_t * buffer, size_t buffer_size)
{
	if (!is_open())
		throw std::runtime_error("device is not open");

	int portion = ::write(_fd, buffer, buffer_size);
	if (portion < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "unable to write to tun device");

	return portion;
}

