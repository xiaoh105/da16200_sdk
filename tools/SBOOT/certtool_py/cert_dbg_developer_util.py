#!/usr/bin/env python3
#
# Copyright (c) 2001-2020, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause OR Arm?�s non-OSI source license
#

import optparse
import configparser
import sys
import os
import logging
sys.path.append(os.path.join(sys.path[0], "..", ".."))

from common import loggerinitializer
from common_cert_lib.developercertificateconfig import DeveloperDebugCertificateConfig
from common_cert_lib.developerdebugarmcertificate import DeveloperDebugArmCertificate

# Util's log file
LOG_FILENAME = "sb_dbg2_cert.log"

# find proj.cfg
'''
if "proj.cfg" in os.listdir(sys.path[0]):
    PROJ_CONFIG_PATH = os.path.join(sys.path[0], "proj.cfg")
elif "proj.cfg" in os.listdir(sys.path[-1]):
    PROJ_CONFIG_PATH = os.path.join(sys.path[-1], "proj.cfg")
else:
    PROJ_CONFIG_PATH = os.path.join(os.getcwd(), "proj.cfg")
'''
def get_path_of_proj_cfg():
    return os.path.join(os.getcwd(), "proj.cfg")

class ArgumentParser:
    def __init__(self):
        self.cfg_filename = None
        self.log_filename = LOG_FILENAME
        self.parser = optparse.OptionParser(usage="usage: %prog cfg_file [log_filename]")

    def parse_arguments(self, sysArgsList):
        (options, args) = self.parser.parse_args(sysArgsList[1:])
        if len(args) > 2 or len(args) < 1:
            self.parser.error("incorrect number of positional arguments")
        elif len(args) == 2:
            self.log_filename = args[1]
        self.cfg_filename = args[0]


class DeveloperDebugCertificateGenerator:
    def __init__(self, developer_dbg_cfg, certificate_version):
        self.config = developer_dbg_cfg
        self.logger = logging.getLogger()
        self.cert_version = certificate_version
        self._certificate = None

    def create_certificate(self):
        self.logger.info("**** Generate debug certificate ****")

        serialized_cert_data = b''
        if self.config.enabler_cert_pkg is not None and self.config.enabler_cert_pkg != "":
            with open(self.config.enabler_cert_pkg, "rb") as enabler_cert_input_file:
                enabler_certificate_data = enabler_cert_input_file.read()
                serialized_cert_data += enabler_certificate_data

        self._certificate = DeveloperDebugArmCertificate(self.config, self.cert_version)
        serialized_cert_data += self._certificate.certificate_data

        with open(self.config.cert_pkg, "wb") as bin_output_file:
            bin_output_file.write(serialized_cert_data)


def CreateDevCertUtility(sysArgsList, PrjDefines):
    if not (sys.version_info.major == 3 and sys.version_info.minor >= 5):
        sys.exit("The script requires Python3.5 or later!")
    # parse arguments
    the_argument_parser = ArgumentParser()
    the_argument_parser.parse_arguments(sysArgsList)
    # get logging up and running
    logger_config = loggerinitializer.LoggerInitializer(the_argument_parser.log_filename)
    logger = logging.getLogger()
    # Get the project configuration values
    project_config = configparser.ConfigParser()
    PROJ_CONFIG_PATH = get_path_of_proj_cfg()
    with open(PROJ_CONFIG_PATH, 'r') as project_config_file:
        config_string = '[PROJ-CFG]\n' + project_config_file.read()
    project_config.read_string(config_string)
    cert_version = [project_config.getint("PROJ-CFG", "CERT_VERSION_MAJOR"),
                    project_config.getint("PROJ-CFG", "CERT_VERSION_MINOR")]
    # get util configuration parameters
    developer_certificate_config = DeveloperDebugCertificateConfig(the_argument_parser.cfg_filename)
    # create certificate
    cert_creator = DeveloperDebugCertificateGenerator(developer_certificate_config, cert_version)
    cert_creator.create_certificate()
    logger.info("**** Certificate file creation has been completed successfully ****")
    del logger_config

##################################
#       Main function
##################################
        
if __name__ == "__main__":

    import sys
    if sys.version_info<(3,0,0):
        print("You need python 3.0 or later to run this script")
        exit(1)

    if "-cfg_file" in sys.argv:
        PROJ_CONFIG = sys.argv[sys.argv.index("-cfg_file") + 1]
    print("Config File  - %s\n" %PROJ_CONFIG)

    # Get the project configuration values
    project_config = configparser.ConfigParser()
    with open(PROJ_CONFIG, 'r') as project_config_file:
        config_string = '[PROJ-CFG]\n' + project_config_file.read()
    project_config.read_string(config_string)
    cert_version = [project_config.getint("PROJ-CFG", "CERT_VERSION_MAJOR"),
                    project_config.getint("PROJ-CFG", "CERT_VERSION_MINOR")]
    
    CreateDevCertUtility(sys.argv, cert_version)
