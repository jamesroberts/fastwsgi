
import os
import sys
import glob
from distutils.command.build_ext import build_ext

SOURCES = glob.glob('libuv/src/*.c')

if sys.platform == 'win32':
    SOURCES += glob.glob("libuv/src/win/*.c")
else:
    SOURCES += [
        'libuv/src/unix/async.c',
        'libuv/src/unix/core.c',
        'libuv/src/unix/dl.c',
        'libuv/src/unix/fs.c',
        'libuv/src/unix/getaddrinfo.c',
        'libuv/src/unix/getnameinfo.c',
        'libuv/src/unix/loop-watcher.c',
        'libuv/src/unix/loop.c',
        'libuv/src/unix/pipe.c',
        'libuv/src/unix/poll.c',
        'libuv/src/unix/process.c',
        'libuv/src/unix/signal.c',
        'libuv/src/unix/stream.c',
        'libuv/src/unix/tcp.c',
        'libuv/src/unix/thread.c',
        'libuv/src/unix/tty.c',
        'libuv/src/unix/udp.c',
        'libuv/src/unix/random-getrandom.c',
        'libuv/src/unix/random-devurandom.c',
    ]

if sys.platform.startswith('linux'):
    SOURCES += [
        'libuv/src/unix/linux-core.c',
        'libuv/src/unix/linux-inotify.c',
        'libuv/src/unix/linux-syscalls.c',
        'libuv/src/unix/procfs-exepath.c',
        'libuv/src/unix/proctitle.c',
        'libuv/src/unix/random-sysctl-linux.c',
        'libuv/src/unix/epoll.c',
    ]
elif sys.platform == 'darwin':
    SOURCES += [
        'libuv/src/unix/bsd-ifaddrs.c',
        'libuv/src/unix/darwin.c',
        'libuv/src/unix/darwin-proctitle.c',
        'libuv/src/unix/fsevents.c',
        'libuv/src/unix/kqueue.c',
        'libuv/src/unix/proctitle.c',
        'libuv/src/unix/pthread-fixes.c',
        'libuv/src/unix/random-getentropy.c',
    ]
elif sys.platform.startswith(('freebsd', 'dragonfly')):
    SOURCES += [
        'libuv/src/unix/bsd-ifaddrs.c',
        'libuv/src/unix/freebsd.c',
        'libuv/src/unix/kqueue.c',
        'libuv/src/unix/posix-hrtime.c',
    ]
elif sys.platform.startswith('openbsd'):
    SOURCES += [
        'libuv/src/unix/bsd-ifaddrs.c',
        'libuv/src/unix/kqueue.c',
        'libuv/src/unix/openbsd.c',
        'libuv/src/unix/posix-hrtime.c',
    ]
elif sys.platform.startswith('netbsd'):
    SOURCES += [
        'libuv/src/unix/bsd-ifaddrs.c',
        'libuv/src/unix/kqueue.c',
        'libuv/src/unix/netbsd.c',
        'libuv/src/unix/posix-hrtime.c',
    ]

elif sys.platform.startswith('sunos'):
    SOURCES += [
        'libuv/src/unix/no-proctitle.c',
        'libuv/src/unix/sunos.c',
    ]


class build_libuv(build_ext):
    libuv_dir = os.path.join('libuv')

    def initialize_options(self):
        build_ext.initialize_options(self)

    def build_extensions(self):
        self.compiler.add_include_dir(os.path.join(self.libuv_dir, 'include'))
        self.compiler.add_include_dir(os.path.join(self.libuv_dir, 'src'))
        self.extensions[0].sources += SOURCES

        if sys.platform != 'win32':
            self.compiler.define_macro('_LARGEFILE_SOURCE', 1)
            self.compiler.define_macro('_FILE_OFFSET_BITS', 64)

        if sys.platform.startswith('linux'):
            self.compiler.add_library('dl')
            self.compiler.add_library('rt')
        elif sys.platform == 'darwin':
            self.compiler.define_macro('_DARWIN_USE_64_BIT_INODE', 1)
            self.compiler.define_macro('_DARWIN_UNLIMITED_SELECT', 1)
        elif sys.platform.startswith('netbsd'):
            self.compiler.add_library('kvm')
        elif sys.platform.startswith('sunos'):
            self.compiler.define_macro('__EXTENSIONS__', 1)
            self.compiler.define_macro('_XOPEN_SOURCE', 500)
            self.compiler.add_library('kstat')
            self.compiler.add_library('nsl')
            self.compiler.add_library('sendfile')
            self.compiler.add_library('socket')
        elif sys.platform == 'win32':
            self.compiler.define_macro('_GNU_SOURCE', 1)
            self.compiler.define_macro('WIN32', 1)
            self.compiler.define_macro('_CRT_SECURE_NO_DEPRECATE', 1)
            self.compiler.define_macro('_CRT_NONSTDC_NO_DEPRECATE', 1)
            self.compiler.define_macro('_WIN32_WINNT', '0x0600')
            self.compiler.add_library('advapi32')
            self.compiler.add_library('iphlpapi')
            self.compiler.add_library('psapi')
            self.compiler.add_library('shell32')
            self.compiler.add_library('user32')
            self.compiler.add_library('userenv')
            self.compiler.add_library('ws2_32')

        build_ext.build_extensions(self)
