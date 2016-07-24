#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
import re

class OpenstackGuestEPAUtils(object):
    """
    Utility class for Host EPA to Openstack flavor extra_specs conversion routines
    """
    def __init__(self):
        self._mano_to_espec_cpu_pinning_policy = {
            'DEDICATED' : 'dedicated',
            'SHARED'    : 'shared',
            'ANY'       : 'any',
        }

        self._espec_to_mano_cpu_pinning_policy = {
            'dedicated' : 'DEDICATED',
            'shared'    : 'SHARED',
            'any'       : 'ANY',
        }
        
        self._mano_to_espec_mempage_size = {
            'LARGE'        : 'large', 
            'SMALL'        : 'small',
            'SIZE_2MB'     :  2048,
            'SIZE_1GB'     :  1048576,
            'PREFER_LARGE' : 'large',
        }

        self._espec_to_mano_mempage_size = {
            'large'        : 'LARGE', 
            'small'        : 'SMALL',
             2048          : 'SIZE_2MB',
             1048576       : 'SIZE_1GB',
            'large'        : 'PREFER_LARGE',
        }

        self._mano_to_espec_cpu_thread_pinning_policy = {
            'AVOID'    : 'avoid',
            'SEPARATE' : 'separate',
            'ISOLATE'  : 'isolate',
            'PREFER'   : 'prefer',
        }

        self._espec_to_mano_cpu_thread_pinning_policy = {
            'avoid'    : 'AVOID',
            'separate' : 'SEPARATE',
            'isolate'  : 'ISOLATE',
            'prefer'   : 'PREFER',
        }

        self._espec_to_mano_numa_memory_policy = {
            'strict'   : 'STRICT',
            'preferred': 'PREFERRED'
        }

        self._mano_to_espec_numa_memory_policy = {
            'STRICT'   : 'strict',
            'PREFERRED': 'preferred'
        }

    def mano_to_extra_spec_cpu_pinning_policy(self, cpu_pinning_policy):
        if cpu_pinning_policy in self._mano_to_espec_cpu_pinning_policy:
            return self._mano_to_espec_cpu_pinning_policy[cpu_pinning_policy]
        else:
            return None

    def extra_spec_to_mano_cpu_pinning_policy(self, cpu_pinning_policy):
        if cpu_pinning_policy in self._espec_to_mano_cpu_pinning_policy:
            return self._espec_to_mano_cpu_pinning_policy[cpu_pinning_policy]
        else:
            return None

    def mano_to_extra_spec_mempage_size(self, mempage_size):
        if mempage_size in self._mano_to_espec_mempage_size:
            return self._mano_to_espec_mempage_size[mempage_size]
        else:
            return None
        
    def extra_spec_to_mano_mempage_size(self, mempage_size):
        if mempage_size in self._espec_to_mano_mempage_size:
            return self._espec_to_mano_mempage_size[mempage_size]
        else:
            return None

    def mano_to_extra_spec_cpu_thread_pinning_policy(self, cpu_thread_pinning_policy):
        if cpu_thread_pinning_policy in self._mano_to_espec_cpu_thread_pinning_policy:
            return self._mano_to_espec_cpu_thread_pinning_policy[cpu_thread_pinning_policy]
        else:
            return None

    def extra_spec_to_mano_cpu_thread_pinning_policy(self, cpu_thread_pinning_policy):
        if cpu_thread_pinning_policy in self._espec_to_mano_cpu_thread_pinning_policy:
            return self._espec_to_mano_cpu_thread_pinning_policy[cpu_thread_pinning_policy]
        else:
            return None

    def mano_to_extra_spec_trusted_execution(self, trusted_execution):
        if trusted_execution:
            return 'trusted'
        else:
            return 'untrusted'

    def extra_spec_to_mano_trusted_execution(self, trusted_execution):
        if trusted_execution == 'trusted':
            return True
        elif trusted_execution == 'untrusted':
            return False
        else:
            return None
        
    def mano_to_extra_spec_numa_node_count(self, numa_node_count):
        return numa_node_count

    def extra_specs_to_mano_numa_node_count(self, numa_node_count):
        return int(numa_node_count)
    
    def mano_to_extra_spec_numa_memory_policy(self, numa_memory_policy):
        if numa_memory_policy in self._mano_to_espec_numa_memory_policy:
            return self._mano_to_espec_numa_memory_policy[numa_memory_policy]
        else:
            return None

    def extra_to_mano_spec_numa_memory_policy(self, numa_memory_policy):
        if numa_memory_policy in self._espec_to_mano_numa_memory_policy:
            return self._espec_to_mano_numa_memory_policy[numa_memory_policy]
        else:
            return None
        
                                                          
    
    
class OpenstackHostEPAUtils(object):
    """
    Utility class for Host EPA to Openstack flavor extra_specs conversion routines
    """
    def __init__(self):
        self._mano_to_espec_cpumodel = {
            "PREFER_WESTMERE"     : "Westmere",
            "REQUIRE_WESTMERE"    : "Westmere",
            "PREFER_SANDYBRIDGE"  : "SandyBridge",
            "REQUIRE_SANDYBRIDGE" : "SandyBridge",
            "PREFER_IVYBRIDGE"    : "IvyBridge",
            "REQUIRE_IVYBRIDGE"   : "IvyBridge",
            "PREFER_HASWELL"      : "Haswell",
            "REQUIRE_HASWELL"     : "Haswell",
            "PREFER_BROADWELL"    : "Broadwell",
            "REQUIRE_BROADWELL"   : "Broadwell",
            "PREFER_NEHALEM"      : "Nehalem",
            "REQUIRE_NEHALEM"     : "Nehalem",
            "PREFER_PENRYN"       : "Penryn",
            "REQUIRE_PENRYN"      : "Penryn",
            "PREFER_CONROE"       : "Conroe",
            "REQUIRE_CONROE"      : "Conroe",
            "PREFER_CORE2DUO"     : "Core2Duo",
            "REQUIRE_CORE2DUO"    : "Core2Duo",
        }

        self._espec_to_mano_cpumodel = {
            "Westmere"     : "REQUIRE_WESTMERE",
            "SandyBridge"  : "REQUIRE_SANDYBRIDGE",
            "IvyBridge"    : "REQUIRE_IVYBRIDGE",
            "Haswell"      : "REQUIRE_HASWELL",
            "Broadwell"    : "REQUIRE_BROADWELL",
            "Nehalem"      : "REQUIRE_NEHALEM",
            "Penryn"       : "REQUIRE_PENRYN",
            "Conroe"       : "REQUIRE_CONROE",
            "Core2Duo"     : "REQUIRE_CORE2DUO",
        }

        self._mano_to_espec_cpuarch = {
            "PREFER_X86"     : "x86",
            "REQUIRE_X86"    : "x86",
            "PREFER_X86_64"  : "x86_64",
            "REQUIRE_X86_64" : "x86_64",
            "PREFER_I686"    : "i686",
            "REQUIRE_I686"   : "i686",
            "PREFER_IA64"    : "ia64",
            "REQUIRE_IA64"   : "ia64",
            "PREFER_ARMV7"   : "ARMv7",
            "REQUIRE_ARMV7"  : "ARMv7",
            "PREFER_ARMV8"   : "ARMv8-A",
            "REQUIRE_ARMV8"  : "ARMv8-A",
        }

        self._espec_to_mano_cpuarch = {
            "x86"     : "REQUIRE_X86",
            "x86_64"  : "REQUIRE_X86_64",
            "i686"    : "REQUIRE_I686",
            "ia64"    : "REQUIRE_IA64",
            "ARMv7-A" : "REQUIRE_ARMV7",
            "ARMv8-A" : "REQUIRE_ARMV8",
        }

        self._mano_to_espec_cpuvendor = {
            "PREFER_INTEL"  : "Intel",
            "REQUIRE_INTEL" : "Intel",
            "PREFER_AMD"    : "AMD",
            "REQUIRE_AMD"   : "AMD",
        }

        self._espec_to_mano_cpuvendor = {
            "Intel" : "REQUIRE_INTEL",
            "AMD"   : "REQUIRE_AMD",
        }

        self._mano_to_espec_cpufeatures = {
            "PREFER_AES"       : "aes",
            "REQUIRE_AES"      : "aes",
            "REQUIRE_VME"      : "vme",
            "PREFER_VME"       : "vme",
            "REQUIRE_DE"       : "de",
            "PREFER_DE"        : "de",
            "REQUIRE_PSE"      : "pse",
            "PREFER_PSE"       : "pse",
            "REQUIRE_TSC"      : "tsc",
            "PREFER_TSC"       : "tsc",
            "REQUIRE_MSR"      : "msr",
            "PREFER_MSR"       : "msr",
            "REQUIRE_PAE"      : "pae",
            "PREFER_PAE"       : "pae",
            "REQUIRE_MCE"      : "mce",
            "PREFER_MCE"       : "mce",
            "REQUIRE_CX8"      : "cx8",
            "PREFER_CX8"       : "cx8",
            "REQUIRE_APIC"     : "apic",
            "PREFER_APIC"      : "apic",
            "REQUIRE_SEP"      : "sep",
            "PREFER_SEP"       : "sep",
            "REQUIRE_MTRR"     : "mtrr",
            "PREFER_MTRR"      : "mtrr",
            "REQUIRE_PGE"      : "pge",
            "PREFER_PGE"       : "pge",
            "REQUIRE_MCA"      : "mca",
            "PREFER_MCA"       : "mca",
            "REQUIRE_CMOV"     : "cmov",
            "PREFER_CMOV"      : "cmov",
            "REQUIRE_PAT"      : "pat",
            "PREFER_PAT"       : "pat",
            "REQUIRE_PSE36"    : "pse36",
            "PREFER_PSE36"     : "pse36",
            "REQUIRE_CLFLUSH"  : "clflush",
            "PREFER_CLFLUSH"   : "clflush",
            "REQUIRE_DTS"      : "dts",
            "PREFER_DTS"       : "dts",
            "REQUIRE_ACPI"     : "acpi",
            "PREFER_ACPI"      : "acpi",
            "REQUIRE_MMX"      : "mmx",
            "PREFER_MMX"       : "mmx",
            "REQUIRE_FXSR"     : "fxsr",
            "PREFER_FXSR"      : "fxsr",
            "REQUIRE_SSE"      : "sse",
            "PREFER_SSE"       : "sse",
            "REQUIRE_SSE2"     : "sse2",
            "PREFER_SSE2"      : "sse2",
            "REQUIRE_SS"       : "ss",
            "PREFER_SS"        : "ss",
            "REQUIRE_HT"       : "ht",
            "PREFER_HT"        : "ht",
            "REQUIRE_TM"       : "tm",
            "PREFER_TM"        : "tm",
            "REQUIRE_IA64"     : "ia64",
            "PREFER_IA64"      : "ia64",
            "REQUIRE_PBE"      : "pbe",
            "PREFER_PBE"       : "pbe",
            "REQUIRE_RDTSCP"   : "rdtscp",
            "PREFER_RDTSCP"    : "rdtscp",
            "REQUIRE_PNI"      : "pni",
            "PREFER_PNI"       : "pni",
            "REQUIRE_PCLMULQDQ": "pclmulqdq",
            "PREFER_PCLMULQDQ" : "pclmulqdq",
            "REQUIRE_DTES64"   : "dtes64",
            "PREFER_DTES64"    : "dtes64",
            "REQUIRE_MONITOR"  : "monitor",
            "PREFER_MONITOR"   : "monitor",
            "REQUIRE_DS_CPL"   : "ds_cpl",
            "PREFER_DS_CPL"    : "ds_cpl",
            "REQUIRE_VMX"      : "vmx",
            "PREFER_VMX"       : "vmx",
            "REQUIRE_SMX"      : "smx",
            "PREFER_SMX"       : "smx",
            "REQUIRE_EST"      : "est",
            "PREFER_EST"       : "est",
            "REQUIRE_TM2"      : "tm2",
            "PREFER_TM2"       : "tm2",
            "REQUIRE_SSSE3"    : "ssse3",
            "PREFER_SSSE3"     : "ssse3",
            "REQUIRE_CID"      : "cid",
            "PREFER_CID"       : "cid",
            "REQUIRE_FMA"      : "fma",
            "PREFER_FMA"       : "fma",
            "REQUIRE_CX16"     : "cx16",
            "PREFER_CX16"      : "cx16",
            "REQUIRE_XTPR"     : "xtpr",
            "PREFER_XTPR"      : "xtpr",
            "REQUIRE_PDCM"     : "pdcm",
            "PREFER_PDCM"      : "pdcm",
            "REQUIRE_PCID"     : "pcid",
            "PREFER_PCID"      : "pcid",
            "REQUIRE_DCA"      : "dca",
            "PREFER_DCA"       : "dca",
            "REQUIRE_SSE4_1"   : "sse4_1",
            "PREFER_SSE4_1"    : "sse4_1",
            "REQUIRE_SSE4_2"   : "sse4_2",
            "PREFER_SSE4_2"    : "sse4_2",
            "REQUIRE_X2APIC"   : "x2apic",
            "PREFER_X2APIC"    : "x2apic",
            "REQUIRE_MOVBE"    : "movbe",
            "PREFER_MOVBE"     : "movbe",
            "REQUIRE_POPCNT"   : "popcnt",
            "PREFER_POPCNT"    : "popcnt",
            "REQUIRE_TSC_DEADLINE_TIMER"   : "tsc_deadline_timer",
            "PREFER_TSC_DEADLINE_TIMER"    : "tsc_deadline_timer",
            "REQUIRE_XSAVE"    : "xsave",
            "PREFER_XSAVE"     : "xsave",
            "REQUIRE_AVX"      : "avx",
            "PREFER_AVX"       : "avx",
            "REQUIRE_F16C"     : "f16c",
            "PREFER_F16C"      : "f16c",
            "REQUIRE_RDRAND"   : "rdrand",
            "PREFER_RDRAND"    : "rdrand",
            "REQUIRE_FSGSBASE" : "fsgsbase",
            "PREFER_FSGSBASE"  : "fsgsbase",
            "REQUIRE_BMI1"     : "bmi1",
            "PREFER_BMI1"      : "bmi1",
            "REQUIRE_HLE"      : "hle",
            "PREFER_HLE"       : "hle",
            "REQUIRE_AVX2"     : "avx2",
            "PREFER_AVX2"      : "avx2",
            "REQUIRE_SMEP"     : "smep",
            "PREFER_SMEP"      : "smep",
            "REQUIRE_BMI2"     : "bmi2",
            "PREFER_BMI2"      : "bmi2",
            "REQUIRE_ERMS"     : "erms",
            "PREFER_ERMS"      : "erms",
            "REQUIRE_INVPCID"  : "invpcid",
            "PREFER_INVPCID"   : "invpcid",
            "REQUIRE_RTM"      : "rtm",
            "PREFER_RTM"       : "rtm",
            "REQUIRE_MPX"      : "mpx",
            "PREFER_MPX"       : "mpx",
            "REQUIRE_RDSEED"   : "rdseed",
            "PREFER_RDSEED"    : "rdseed",
            "REQUIRE_ADX"      : "adx",
            "PREFER_ADX"       : "adx",
            "REQUIRE_SMAP"     : "smap",
            "PREFER_SMAP"      : "smap",
        }

        self._espec_to_mano_cpufeatures = {
            "aes"      : "REQUIRE_AES",
            "vme"      : "REQUIRE_VME",
            "de"       : "REQUIRE_DE",
            "pse"      : "REQUIRE_PSE",
            "tsc"      : "REQUIRE_TSC",
            "msr"      : "REQUIRE_MSR",
            "pae"      : "REQUIRE_PAE",
            "mce"      : "REQUIRE_MCE",
            "cx8"      : "REQUIRE_CX8",
            "apic"     : "REQUIRE_APIC",
            "sep"      : "REQUIRE_SEP",
            "mtrr"     : "REQUIRE_MTRR",
            "pge"      : "REQUIRE_PGE",
            "mca"      : "REQUIRE_MCA",
            "cmov"     : "REQUIRE_CMOV",
            "pat"      : "REQUIRE_PAT",
            "pse36"    : "REQUIRE_PSE36",
            "clflush"  : "REQUIRE_CLFLUSH",
            "dts"      : "REQUIRE_DTS",
            "acpi"     : "REQUIRE_ACPI",
            "mmx"      : "REQUIRE_MMX",
            "fxsr"     : "REQUIRE_FXSR",
            "sse"      : "REQUIRE_SSE",
            "sse2"     : "REQUIRE_SSE2",
            "ss"       : "REQUIRE_SS",
            "ht"       : "REQUIRE_HT",
            "tm"       : "REQUIRE_TM",
            "ia64"     : "REQUIRE_IA64",
            "pbe"      : "REQUIRE_PBE",
            "rdtscp"   : "REQUIRE_RDTSCP",
            "pni"      : "REQUIRE_PNI",
            "pclmulqdq": "REQUIRE_PCLMULQDQ",
            "dtes64"   : "REQUIRE_DTES64",
            "monitor"  : "REQUIRE_MONITOR",
            "ds_cpl"   : "REQUIRE_DS_CPL",
            "vmx"      : "REQUIRE_VMX",
            "smx"      : "REQUIRE_SMX",
            "est"      : "REQUIRE_EST",
            "tm2"      : "REQUIRE_TM2",
            "ssse3"    : "REQUIRE_SSSE3",
            "cid"      : "REQUIRE_CID",
            "fma"      : "REQUIRE_FMA",
            "cx16"     : "REQUIRE_CX16",
            "xtpr"     : "REQUIRE_XTPR",
            "pdcm"     : "REQUIRE_PDCM",
            "pcid"     : "REQUIRE_PCID",
            "dca"      : "REQUIRE_DCA",
            "sse4_1"   : "REQUIRE_SSE4_1",
            "sse4_2"   : "REQUIRE_SSE4_2",
            "x2apic"   : "REQUIRE_X2APIC",
            "movbe"    : "REQUIRE_MOVBE",
            "popcnt"   : "REQUIRE_POPCNT",
            "tsc_deadline_timer"   : "REQUIRE_TSC_DEADLINE_TIMER",
            "xsave"    : "REQUIRE_XSAVE",
            "avx"      : "REQUIRE_AVX",
            "f16c"     : "REQUIRE_F16C",
            "rdrand"   : "REQUIRE_RDRAND",
            "fsgsbase" : "REQUIRE_FSGSBASE",
            "bmi1"     : "REQUIRE_BMI1",
            "hle"      : "REQUIRE_HLE",
            "avx2"     : "REQUIRE_AVX2",
            "smep"     : "REQUIRE_SMEP",
            "bmi2"     : "REQUIRE_BMI2",
            "erms"     : "REQUIRE_ERMS",
            "invpcid"  : "REQUIRE_INVPCID",
            "rtm"      : "REQUIRE_RTM",
            "mpx"      : "REQUIRE_MPX",
            "rdseed"   : "REQUIRE_RDSEED",
            "adx"      : "REQUIRE_ADX",
            "smap"     : "REQUIRE_SMAP",
        }

    def mano_to_extra_spec_cpu_model(self, cpu_model):
        if cpu_model in self._mano_to_espec_cpumodel:
            return self._mano_to_espec_cpumodel[cpu_model]
        else:
            return None
            
    def extra_specs_to_mano_cpu_model(self, cpu_model):
        if cpu_model in self._espec_to_mano_cpumodel:
            return self._espec_to_mano_cpumodel[cpu_model]
        else:
            return None
        
    def mano_to_extra_spec_cpu_arch(self, cpu_arch):
        if cpu_arch in self._mano_to_espec_cpuarch:
            return self._mano_to_espec_cpuarch[cpu_arch]
        else:
            return None
        
    def extra_specs_to_mano_cpu_arch(self, cpu_arch):
        if cpu_arch in self._espec_to_mano_cpuarch:
            return self._espec_to_mano_cpuarch[cpu_arch]
        else:
            return None
    
    def mano_to_extra_spec_cpu_vendor(self, cpu_vendor):
        if cpu_vendor in self._mano_to_espec_cpuvendor:
            return self._mano_to_espec_cpuvendor[cpu_vendor]
        else:
            return None

    def extra_spec_to_mano_cpu_vendor(self, cpu_vendor):
        if cpu_vendor in self._espec_to_mano_cpuvendor:
            return self._espec_to_mano_cpuvendor[cpu_vendor]
        else:
            return None
    
    def mano_to_extra_spec_cpu_socket_count(self, cpu_sockets):
        return cpu_sockets

    def extra_spec_to_mano_cpu_socket_count(self, cpu_sockets):
        return int(cpu_sockets)
    
    def mano_to_extra_spec_cpu_core_count(self, cpu_core_count):
        return cpu_core_count

    def extra_spec_to_mano_cpu_core_count(self, cpu_core_count):
        return int(cpu_core_count)
    
    def mano_to_extra_spec_cpu_core_thread_count(self, core_thread_count):
        return core_thread_count

    def extra_spec_to_mano_cpu_core_thread_count(self, core_thread_count):
        return int(core_thread_count)

    def mano_to_extra_spec_cpu_features(self, features):
        cpu_features = []
        epa_feature_str = None
        for f in features:
            if f in self._mano_to_espec_cpufeatures:
                cpu_features.append(self._mano_to_espec_cpufeatures[f])
                
        if len(cpu_features) > 1:
            epa_feature_str =  '<all-in> '+ " ".join(cpu_features)
        elif len(cpu_features) == 1:
            epa_feature_str = " ".join(cpu_features)

        return epa_feature_str

    def extra_spec_to_mano_cpu_features(self, features):
        oper_symbols = ['=', '<in>', '<all-in>', '==', '!=', '>=', '<=', 's==', 's!=', 's<', 's<=', 's>', 's>=']
        cpu_features = []
        result = None
        for oper in oper_symbols:
            regex = '^'+oper+' (.*?)$'
            result = re.search(regex, features)
            if result is not None:
                break
            
        if result is not None:
            feature_list = result.group(1)
        else:
            feature_list = features

        for f in feature_list.split():
            if f in self._espec_to_mano_cpufeatures:
                cpu_features.append(self._espec_to_mano_cpufeatures[f])

        return cpu_features
    

class OpenstackExtraSpecUtils(object):
    """
    General utility class for flavor Extra Specs processing
    """
    def __init__(self):
        self.host = OpenstackHostEPAUtils()
        self.guest = OpenstackGuestEPAUtils()
        self.extra_specs_keywords = [ 'hw:cpu_policy',
                                      'hw:cpu_threads_policy',
                                      'hw:mem_page_size',
                                      'hw:numa_nodes',
                                      'hw:numa_mempolicy',
                                      'hw:numa_cpus',
                                      'hw:numa_mem',
                                      'trust:trusted_host',
                                      'pci_passthrough:alias',
                                      'capabilities:cpu_info:model',
                                      'capabilities:cpu_info:arch',
                                      'capabilities:cpu_info:vendor',
                                      'capabilities:cpu_info:topology:sockets',
                                      'capabilities:cpu_info:topology:cores',
                                      'capabilities:cpu_info:topology:threads',
                                      'capabilities:cpu_info:features',
                                ]
        self.extra_specs_regex = re.compile("^"+"|^".join(self.extra_specs_keywords))
