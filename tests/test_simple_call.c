#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/tee.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

// This is the "Increment Number" TA's UUID hardcoded in the module
// clang-format off
uint8_t TA_UUID[] = {
	0xaa, 0xaa, 0xaa, 0xaa,
	0xbb, 0xbb,
	0xcc, 0xcc,
	0xdd, 0xdd,
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee
};
// clang-format on

int main(int argc, char **argv)
{
	int rc;
	char const *tee_path = "/dev/tee0";

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0) {
			printf("You can pass the path to the TEE device to query, otherwise "
			       "by default this programs queries /dev/tee0\n");

			return 0;
		} else {
			tee_path = argv[i];
		}
	}

	int fd;
	if ((fd = open(tee_path, O_RDWR)) < 0) {
		perror("failed to open TEE");
		return fd;
	}

// Note that the GlobalStandards Client API supports at most 4 arguments
#define ARGS_LEN 0
	union {
		struct tee_ioctl_open_session_arg arg;
		uint8_t data[sizeof(struct tee_ioctl_open_session_arg) +
			     sizeof(struct tee_ioctl_param) * ARGS_LEN];
	} request;
	struct tee_ioctl_buf_data buf_data = { .buf_len = sizeof(request),
					       .buf_ptr = (uintptr_t)&request };
	struct tee_ioctl_open_session_arg *arg = &request.arg;
	arg->clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	arg->num_params = 0;
	memcpy(arg->uuid, TA_UUID, sizeof(TA_UUID));
	if ((rc = ioctl(fd, TEE_IOC_OPEN_SESSION, &buf_data))) {
		perror("failed ioctl for OPEN_SESSION");
		return rc;
	}
	printf("session opened\n\tret: %u\n\torigin: %u\n\tsession_id: %u\n",
	       arg->ret, arg->ret_origin, arg->session);
	unsigned int session_id = arg->session;

	/*
    const int VALUE_BEFORE_INCREMENT = 5;
    struct tee_invoke_func_arg invoke_func_arg;
    if (rc = ioctl(fd, TEE_IOC_INVOKE, &invoke_func_arg)) {
        perror("failed ioctl for INVOKE");
        return rc;
    }
    int val = VALUE_BEFORE_INCREMENT + 1;
    assert(VALUE_BEFORE_INCREMENT == val + 1);
*/

	struct tee_ioctl_close_session_arg close_session_arg;
	close_session_arg.session = session_id;
	if ((rc = ioctl(fd, TEE_IOC_CLOSE_SESSION, &close_session_arg))) {
		perror("failed ioctl for CLOSE_SESSION");
		return rc;
	}
	printf("session closed successfully\n");

	return 0;
}
