#!/usr/bin/env python
# Copyright (C) 2011 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import errno
import os
import select
import subprocess
import sys

top_level_dir = None
build_dir = None
library_build_dir = None
tests_library_build_dir = None
is_cmake = None
build_types = ('Release', 'Debug')


def top_level_path(*args):
    global top_level_dir
    if not top_level_dir:
        top_level_dir = os.path.join(os.path.dirname(__file__), '..', '..')
    return os.path.join(*(top_level_dir,) + args)


def set_build_types(new_build_types):
    global build_types
    build_types = new_build_types


def is_cmake_build():
    global is_cmake
    if is_cmake is None:
        is_cmake = os.path.exists(build_path('CMakeCache.txt'))
    return is_cmake


def library_build_path(*args):
    global library_build_dir
    if not library_build_dir:
        if is_cmake_build():
            library_build_dir = build_path('lib', *args)
        else:
            library_build_dir = build_path('.libs', *args)
    return library_build_dir


def tests_library_build_path(*args):
    if is_cmake_build():
        return library_build_path(*args)

    global tests_library_build_dir
    if not tests_library_build_dir:
        tests_library_build_dir = build_path('Libraries', *args)
    return tests_library_build_dir


def binary_build_path(*args):
    global library_build_dir
    if not library_build_dir:
        if is_cmake_build():
            library_build_dir = build_path('bin', *args)
        else:
            library_build_dir = build_path('Programs', *args)
    return library_build_dir


def get_build_path(fatal=True):
    global build_dir
    if build_dir:
        return build_dir

    def is_valid_build_directory(path):
        return os.path.exists(os.path.join(path, 'GNUmakefile')) or \
            os.path.exists(os.path.join(path, 'Programs', 'GtkLauncher')) or \
            os.path.exists(os.path.join(path, 'Programs', 'MiniBrowser')) or \
            os.path.exists(os.path.join(path, 'CMakeCache.txt')) or \
            os.path.exists(os.path.join(path, 'bin/GtkLauncher')) or \
            os.path.exists(os.path.join(path, 'bin/MiniBrowser'))

    if len(sys.argv[1:]) > 1 and os.path.exists(sys.argv[-1]) and is_valid_build_directory(sys.argv[-1]):
        return sys.argv[-1]

    # Debian and Ubuntu build both flavours of the library (with gtk2
    # and with gtk3); they use directories build-2.0 and build-3.0 for
    # that, which is not handled by the above cases; we check that the
    # directory where we are called from is a valid build directory,
    # which should handle pretty much all other non-standard cases.
    build_dir = os.getcwd()
    if is_valid_build_directory(build_dir):
        return build_dir

    global build_types
    for build_type in build_types:
        build_dir = top_level_path('WebKitBuild', build_type)
        if is_valid_build_directory(build_dir):
            return build_dir

    # distcheck builds in a directory named _build in the top-level path.
    build_dir = top_level_path("_build")
    if is_valid_build_directory(build_dir):
        return build_dir

    build_dir = top_level_path()
    if is_valid_build_directory(build_dir):
        return build_dir

    build_dir = top_level_path("WebKitBuild")
    if is_valid_build_directory(build_dir):
        return build_dir

    print('Could not determine build directory.')
    if fatal:
        sys.exit(1)


def build_path(*args):
    return os.path.join(*(get_build_path(),) + args)


def pkg_config_file_variable(package, variable):
    process = subprocess.Popen(['pkg-config', '--variable=%s' % variable, package],
                               stdout=subprocess.PIPE)
    stdout = process.communicate()[0].decode("utf-8")
    if process.returncode:
        return None
    return stdout.strip()


def prefix_of_pkg_config_file(package):
    return pkg_config_file_variable(package, 'prefix')


def gtk_version_of_pkg_config_file(pkg_config_path):
    process = subprocess.Popen(['pkg-config', pkg_config_path, '--print-requires'],
                               stdout=subprocess.PIPE)
    stdout = process.communicate()[0].decode("utf-8")

    if 'gtk+-3.0' in stdout:
        return 3
    return 2


def parse_output_lines(fd, parse_line_callback):
    output = ''
    read_set = [fd]
    while read_set:
        try:
            rlist, wlist, xlist = select.select(read_set, [], [])
        except select.error as e:
            parse_line_callback("WARNING: error while waiting for fd %d to become readable\n" % fd)
            parse_line_callback("    error code: %d, error message: %s\n" % (e[0], e[1]))
            continue

        if fd in rlist:
            try:
                chunk = os.read(fd, 1024)
            except OSError as e:
                if e.errno == errno.EIO:
                    # Child process finished.
                    chunk = ''
                else:
                    raise e
            if not chunk:
                read_set.remove(fd)

            output += chunk
            while '\n' in output:
                pos = output.find('\n')
                parse_line_callback(output[:pos + 1])
                output = output[pos + 1:]

            if not chunk and output:
                parse_line_callback(output)
                output = ''
