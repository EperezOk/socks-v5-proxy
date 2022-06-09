CFLAGS := -std=c11 -pedantic -pedantic-errors -Wall -fsanitize=address -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -pthread

TARGET_CLIENT := client
TARGET_SERVER := socks5d
