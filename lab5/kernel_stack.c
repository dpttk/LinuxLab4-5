#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#define STACK_DEVICE_PATH    "/dev/int_stack"
#define STACK_CONFIG_CMD     _IOW('s', 1, int)

#define EXIT_CONFIG_ERROR    2
#define EXIT_IO_ERROR        3
#define EXIT_FORMAT_ERROR    4
#define EXIT_USB_ERROR       5 

static int device_handle = -1;

static void release_resources(void);
static void show_help(const char *program_name);
static int configure_stack_size(const char *size_str);
static int add_value_to_stack(const char *value_str);
static int retrieve_value_from_stack(void);
static int empty_entire_stack(void);

int main(int argc, char *argv[])
{
    int status = EXIT_SUCCESS;
    
    if (argc < 2) {
        show_help(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (atexit(release_resources) != 0) {
        fprintf(stderr, "Warning: Could not register cleanup handler\n");
    }
    
    device_handle = open(STACK_DEVICE_PATH, O_RDWR);
    if (device_handle < 0) {
        if (errno == ENODEV) {
            fprintf(stderr, "Error: USB key not inserted\n");
            return EXIT_USB_ERROR;
        } else {
            fprintf(stderr, "Error: Failed to open stack device: %s\n", 
                    strerror(errno));
            return EXIT_IO_ERROR;
        }
    }
    
    const char *command = argv[1];
    
    if (strcmp(command, "set-size") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Error: The set-size command requires a size argument\n");
            return EXIT_FAILURE;
        }
        status = configure_stack_size(argv[2]);
    }
    else if (strcmp(command, "push") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Error: The push command requires a value argument\n");
            return EXIT_FAILURE;
        }
        status = add_value_to_stack(argv[2]);
    }
    else if (strcmp(command, "pop") == 0) {
        status = retrieve_value_from_stack();
    }
    else if (strcmp(command, "unwind") == 0) {
        status = empty_entire_stack();
    }
    else {
        fprintf(stderr, "Error: Unknown command: %s\n", command);
        show_help(argv[0]);
        status = EXIT_FAILURE;
    }
    
    return status;
}

static void release_resources(void)
{
    if (device_handle >= 0) {
        close(device_handle);
        device_handle = -1;
    }
}

static void show_help(const char *program_name)
{
    printf("Usage: %s <command> [arguments]\n\n", program_name);
    printf("Available commands:\n");
    printf("  set-size <size>  Configure the maximum stack capacity\n");
    printf("  push <value>     Add an integer to the stack\n");
    printf("  pop              Remove and display the top stack element\n");
    printf("  unwind           Remove and display all stack elements\n");
}

static int configure_stack_size(const char *size_str)
{
    char *endptr;
    long size_value;
    
    size_value = strtol(size_str, &endptr, 10);
    if (*endptr != '\0' || size_value <= 0) {
        fprintf(stderr, "Error: Stack size must be a positive number\n");
        return EXIT_FORMAT_ERROR;
    }
    
    if (ioctl(device_handle, STACK_CONFIG_CMD, &size_value) != 0) {
        switch (errno) {
            case ENODEV:
                fprintf(stderr, "Error: USB key not inserted\n");
                return EXIT_USB_ERROR;
            case EBUSY:
                fprintf(stderr, "Error: Stack size has already been configured\n");
                break;
            case EINVAL:
                fprintf(stderr, "Error: Specified size value is invalid\n");
                break;
            default:
                fprintf(stderr, "Error: Failed to configure stack size: %s\n", 
                        strerror(errno));
        }
        return EXIT_CONFIG_ERROR;
    }
    
    return EXIT_SUCCESS;
}

static int add_value_to_stack(const char *value_str)
{
    char *endptr;
    int int_value;
    
    long temp_value = strtol(value_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Input must be a valid integer\n");
        return EXIT_FORMAT_ERROR;
    }
    
    int_value = (int)temp_value;
    
    if (write(device_handle, &int_value, sizeof(int_value)) != sizeof(int_value)) {
        if (errno == ENODEV) {
            fprintf(stderr, "Error: USB key not inserted\n");
            return EXIT_USB_ERROR;
        } else if (errno == ENOSPC || errno == ERANGE) {
            fprintf(stderr, "Error: Stack is full\n");
        } else {
            fprintf(stderr, "Error: Failed to write to stack: %s\n", 
                    strerror(errno));
        }
        return EXIT_IO_ERROR;
    }
    
    return EXIT_SUCCESS;
}

static int retrieve_value_from_stack(void)
{
    int value;
    ssize_t bytes_read;
    bytes_read = read(device_handle, &value, sizeof(value));
    
    if (bytes_read == 0) {
        printf("Stack is empty\n");
    } else if (bytes_read == sizeof(value)) {
        printf("%d\n", value);
    } else {
        if (errno == ENODEV) {
            fprintf(stderr, "Error: USB key not inserted\n");
            return EXIT_USB_ERROR;
        } else {
            fprintf(stderr, "Error: Failed to read from stack: %s\n", 
                    strerror(errno));
        }
        return EXIT_IO_ERROR;
    }
    
    return EXIT_SUCCESS;
}

static int empty_entire_stack(void)
{
    int count = 0;
    int value;
    ssize_t bytes_read;
    
    while (1) {
        bytes_read = read(device_handle, &value, sizeof(value));
        
        if (bytes_read == 0) {
            if (count == 0) {
                printf("Stack is empty\n");
            }
            break;
        } else if (bytes_read != sizeof(value)) {
            if (errno == ENODEV) {
                fprintf(stderr, "Error: USB key not inserted\n");
                return EXIT_USB_ERROR;
            } else {
                fprintf(stderr, "Error: Failed to read from stack: %s\n", 
                        strerror(errno));
            }
            return EXIT_IO_ERROR;
        }
        
        printf("%d\n", value);
        count++;
    }
    
    return EXIT_SUCCESS;
}
