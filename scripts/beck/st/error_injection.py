#!/usr/bin/python3
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#
import binascii
import sys
import os
import random
import argparse
import time
import logging

timestr = time.strftime("%Y%m%d-%H%M%S")
log_filename = "hole_creation_" + timestr + ".log"

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

fh = logging.FileHandler(log_filename)
fh.setLevel(logging.DEBUG)

ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
fformatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
cformatter = logging.Formatter('%(levelname)s : %(message)s')
fh.setFormatter(fformatter)
ch.setFormatter(cformatter)
logger.addHandler(fh)
logger.addHandler(ch)

logger.info("***** Script Started *****")

parser = argparse.ArgumentParser(description="Basic Arguments to run the script")
parser.add_argument('-rn', action='store_true', default=False, dest='random', help='For inducing error at Random place')
parser.add_argument('-e', action='store', default=0, type=int, dest='noOfErr',
                    help='How Many number of error do you want to induce in Metadata')
parser.add_argument('-rt', action='store', dest='Record_Type',
                    help='Record Type For inducing error at perticular record like BE_BTREE, BE_EMAP, CAS_CTG etc')
parser.add_argument('-m', action='store', dest='mfile', help='Metadata Path')
parser.add_argument('-v', action='store_true', default=False, dest='verify',
                    help='Read full Metadata and Print all the Records entries counts')
parser.add_argument('-a', action='store_true', default=False, dest='allErr',
                    help='Induce Error in All Record at Random place')
parser.add_argument('-gmd', action='store_true', default=False, dest='allGMD',
                    help='Induce Error in All GMD type of Record at Random place')
parser.add_argument('-dmd', action='store_true', default=False, dest='allDMD',
                    help='Induce Error in All DMD type of Record at Random place')
parser.add_argument('-512k', action='store_true', default=False, dest='err512k',
                    help='Induce 512K bytes error in Metadata')
parser.add_argument('-huge', action='store_true', default=False, dest='hugeCorruption',
                    help='Induce Huge amount of corruption in Metadata')
parser.add_argument('-seed', action='store', default=0, type=float, dest='seed',
                    help='Seed is used to initialize the "random" library: to initialize the random generation')
parser.add_argument('-emap', action='store', dest='corrupt_emap', help='Induce Error in Emap specified by Cob Id')
parser.add_argument('-list_emap', action='store_true', default=False, dest='list_emap', help='Display all Emap keys with device id')

args = parser.parse_args()

results = parser.parse_args()
logger.info('Induce Random Error            = {!r}'.format(args.random))
logger.info('Number of Error induce         = {!r}'.format(args.noOfErr))
logger.info('Record Type                    = {!r}'.format(args.Record_Type))
logger.info('Metadata file path             = {!r}'.format(args.mfile))
logger.info('Verify Record entries          = {!r}'.format(args.verify))
logger.info('Induce Error in All Record     = {!r}'.format(args.allErr))
logger.info('Induce Error in GMD Record     = {!r}'.format(args.allGMD))
logger.info('Induce Error in DMD Record     = {!r}'.format(args.allDMD))
logger.info('Induce 512k errors             = {!r}'.format(args.err512k))
logger.info('Induce huge errors             = {!r}'.format(args.hugeCorruption))
logger.info('Seed for random number         = {!r}'.format(args.seed))
logger.info('Induce Error in emap by Cob Id = {!r}'.format(args.corrupt_emap))
logger.info('List all Emap Keys and Records = {!r}'.format(args.list_emap))

filename = args.mfile
recordType = args.Record_Type
noOfErr = args.noOfErr

if args.seed != 0:
    seed = args.seed
    logger.info("Seed used: {}".format(seed))
else:
    seed = time.time()
    logger.info("Seed used: {}".format(seed))

random.seed(seed)

if not os.walk(filename):
    logger.error('Failed: The path specified does not exist or Missing file path')
    sys.exit(1)

# M0_FORMAT_HEADER_MAGIC = 0x33011ca5e511de77
header = b'33011ca5e511de77'
# M0_FORMAT_FOOTER_MAGIC = 0x33f007e7f007e777
footer = b'33f007e7f007e777'

typeDict = {b'01': 'RPC_PACKET', b'02': 'RPC_ITEM', b'03': 'BE_BTREE', b'04': 'BE_BNODE', b'05': 'BE_EMAP_KEY',
            b'06': 'BE_EMAP_REC',
            b'07': 'BE_EMAP', b'08': 'BE_LIST', b'09': 'BE_SEG_HDR', b'0a': 'BALLOC', b'0b': 'ADDB2_FRAME_HEADER',
            b'0c': 'STOB_AD_0TYPE_REC',
            b'0d': 'STOB_AD_DOMAIN', b'0e': 'COB_DOMAIN', b'0f': 'COB_NSREC', b'10': 'BALLOC_GROUP_DESC', b'11': 'EXT',
            b'12': 'CAS_INDEX',
            b'13': 'POOLNODE', b'14': 'POOLDEV', b'15': 'POOL_SPARE_USAGE', b'16': 'CAS_STATE', b'17': 'CAS_CTG',
            b'22': 'WRONG_ENTRY', b'44': 'WRONG_ENTRY'}

recordDict = {'BE_BTREE': [], 'BE_BNODE': [], 'BE_EMAP_KEY': [], 'BE_EMAP_REC': [], 'BE_EMAP': [], 'BE_LIST': [],
              'BE_SEG_HDR': [], 'BALLOC': [],
              'STOB_AD_0TYPE_REC': [], 'STOB_AD_DOMAIN': [], 'COB_DOMAIN': [], 'COB_NSREC': [], 'BALLOC_GROUP_DESC': [],
              'EXT': [], 'POOLNODE': [],
              'POOLDEV': [], 'POOL_SPARE_USAGE': [], 'CAS_STATE': [], 'CAS_CTG': [], 'EXTRA': []}

sizeDict = {'BE_BTREE': [], 'BE_BNODE': [], 'BE_EMAP_KEY': [], 'BE_EMAP_REC': [], 'BE_EMAP': [], 'BE_LIST': [],
            'BE_SEG_HDR': [], 'BALLOC': [],
            'STOB_AD_0TYPE_REC': [], 'STOB_AD_DOMAIN': [], 'COB_DOMAIN': [], 'COB_NSREC': [], 'BALLOC_GROUP_DESC': [],
            'EXT': [], 'POOLNODE': [],
            'POOLDEV': [], 'POOL_SPARE_USAGE': [], 'CAS_STATE': [], 'CAS_CTG': [], 'EXTRA': []}

DMDList = ['BE_BNODE', 'BE_EMAP_KEY', 'BE_EMAP_REC', 'COB_NSREC', 'BALLOC_GROUP_DESC']
GMDList = ['BE_BTREE', 'BE_EMAP', 'BE_LIST', 'BE_SEG_HDR', 'BALLOC', 'STOB_AD_0TYPE_REC', 'STOB_AD_DOMAIN',
           'COB_DOMAIN', 'CAS_STATE', 'CAS_CTG']

btreeType = {b'01': 'M0_BBT_INVALID', b'02': 'M0_BBT_BALLOC_GROUP_EXTENTS', b'03': 'M0_BBT_BALLOC_GROUP_DESC', b'04': 'M0_BBT_EMAP_EM_MAPPING',
             b'05': 'M0_BBT_CAS_CTG', b'06': 'M0_BBT_COB_NAMESPACE', b'07': 'M0_BBT_COB_OBJECT_INDEX', b'08': 'M0_BBT_COB_FILEATTR_BASIC',
             b'09': 'M0_BBT_COB_FILEATTR_EA', b'0a': 'M0_BBT_COB_FILEATTR_OMG', b'0b': 'M0_BBT_CONFDB', b'0c': 'M0_BBT_UT_KV_OPS', b'0d': 'M0_BBT_NR'}

BeBnodeTypeKeys = {}

def RecordOffset(record, i, size):
    if record in recordDict.keys():
        recordDict[record].append(i)
        sizeDict[record].append(size)
        if record == "BE_BNODE":
            bliType = i + 16                              # bli_type offet
            btNumActiveKey = i + 56                       # active key count offset
            BeBnodeTypeKeys[i] = [bliType, btNumActiveKey]
    else:
        recordDict['EXTRA'].append(i)
        sizeDict['EXTRA'].append(size)

def ReadTypeSize(byte):  # Ex: 0001(ver) 0009(type) 00003dd8(size)
    # ver = byte[:4]   # .ot_version = src->hd_bits >> 48,
    rtype = byte[6:8]  # .ot_type    = src->hd_bits >> 32 & 0x0000ffff,
    size = byte[8:16]  # .ot_size    = src->hd_bits & 0xffffffff
    # logger.info("Version {}, Type {}, Size {}".format(ver, rtype, size))  #debug print
    return rtype, size


def EditMetadata(offset):
    with open(filename, 'r+b') as wbfr:
        logger.info("** Corrupting 8byte of Metadata at offset {} with b'1111222244443333' **".format(offset))
        wbfr.seek(offset)
        wbfr.write(b'\x33\x33\x44\x44\x22\x22\x11\x11')
        wbfr.seek(offset)
        ReadMetadata(offset)


# If you want to verify the written Metadata then run below segment of code
def ReadMetadata(offset):
    with open(filename, "rb") as mdata:
        mdata.seek(offset)
        data = binascii.hexlify((mdata.read(8))[::-1])
        if data == footer:
            return True, data
        else:
            return False, data

# This function returns complete data with list format in 8 bytes chunks,
# starting after header and until footer for record
def ReadCompleteRecord(offset):
    curr_record = []
    while 1:
        footerFound, data=ReadMetadata(offset)
        if footerFound:
            break
        curr_record.append(data.decode('utf-8'))
        offset = offset + 8 # check next 8 bytes

    # Convert list to hex representation
    curr_record = [ hex(int(i, 16)) for i in curr_record]
    return curr_record, offset # Return record data and footer offset

def ReadBeBNode(offset):
    llist = BeBnodeTypeKeys[offset]
    with open(filename, "rb") as mdata:
        mdata.seek(llist[0])
        data = binascii.hexlify((mdata.read(8))[::-1])
        data = data[14:16]
        logger.info("bli_type of BE_BNODE is: {0}: {1}".format( data, btreeType[data]))

        mdata.seek(llist[1])
        data = binascii.hexlify((mdata.read(8))[::-1])
        data = data[8:16]
        logger.info("Active key count of BE_BNODE is: {}".format( int(data,16)))

def InduceCorruption(recordType, noOfErr):
    count = 0
    read_metadata_file()
    logger.info(recordType)
    logger.info("Number of Error want to induce: {}".format(noOfErr))
    lookupList = recordDict[recordType]
    if (len(lookupList) and noOfErr) == 0:
        logger.error("Record List is empty. Please choose another Record")
        count = 0
        return count
    elif len(lookupList) < noOfErr:
        logger.error(
            " Record List contains Less number of entries than input. Please reduce the number of Error Injection")
        count = 0
        return count
    else:
        logger.info(lookupList)
        logger.info("**** Inducing {} Error in Record: {} ****".format(noOfErr, recordType))
        for i in range(noOfErr):
            offset = lookupList[i]  # Please add offset here for starting from middle of offset list
            ReadMetadata(offset + 8)
            EditMetadata(offset + 8)
            if recordType == "BE_BNODE":
                ReadBeBNode(offset)
            count = count + 1
    return count


def InduceRandomCorruption(noOfErr):
    count = 0
    read_metadata_file()
    while 1:
        recType = random.choice(list(recordDict))
        logger.info("++++ Picked a Random Record from Dictionary Record type:{}++++".format(recType))
        logger.info("Number of Error want to induce: {}".format(noOfErr))
        lookupList = recordDict[recType]
        logger.info(lookupList)
        if (len(lookupList) == 0) or (len(lookupList) < noOfErr):
            logger.info("Record List is empty OR contains Less number of entries than input. Going to next Record")
        else:
            lookupList = random.sample(lookupList, noOfErr)
            logger.info(lookupList)
            for i in range(noOfErr):
                offset = lookupList[i]
                logger.info("**** Inducing RANDOM Error in Record at offsets {}****".format(hex(offset + 8)))
                ReadMetadata(offset + 8)  # Read original
                EditMetadata(offset + 8)  # Modify
                ReadMetadata(offset + 8)  # Verify
                count = count + 1
            break
    return count


def InduceErrInAllRecord():
    count = 0
    read_metadata_file()
    logger.info("++++ Induce Random number of errors in All Records ++++")
    for recType in recordDict:
        logger.info("Record Name: {}".format(recType))
        lookupList = recordDict[recType]
        length = len(lookupList)
        if length == 0:
            logger.info("Record List is empty. Moving to Next Record")
        else:
            lookupList = random.sample(lookupList, random.randint(1, length))
            logger.info("Inducing {} Error at these offsets".format(len(lookupList)))
            logger.info(lookupList)
            for offset in lookupList:
                logger.info("**** Inducing Error in Record at offsets {}****".format(hex(offset + 8)))
                ReadMetadata(offset + 8)  # Read original
                EditMetadata(offset + 8)  # Modify
                ReadMetadata(offset + 8)  # Verify
                count = count + 1
    return count


def InduceErrInGMDRecords():
    count = 0
    read_metadata_file()
    logger.info("++++ Induce Random number of errors in All GMD Records ++++")
    for recType in GMDList:
        logger.info("Record Name: {}".format(recType))
        lookupList = recordDict[recType]
        length = len(lookupList)
        if length == 0:
            logger.info("Record List is empty. Moving to Next Record")
        else:
            lookupList = random.sample(lookupList, random.randint(1, length))
            logger.info("Inducing {} Error at these offsets".format(len(lookupList)))
            logger.info(lookupList)
            for offset in lookupList:
                logger.info("**** Inducing Error in Record at offsets {}****".format(hex(offset + 8)))
                ReadMetadata(offset + 8)  # Read original
                EditMetadata(offset + 8)  # Modify
                ReadMetadata(offset + 8)  # Verify
                count = count + 1
    return count


def InduceErrInDMDRecords():
    count = 0
    read_metadata_file()
    logger.info("++++ Induce Random number of errors in All DMD Records ++++")
    for recType in DMDList:
        logger.info("Record Name: {}".format(recType))
        lookupList = recordDict[recType]
        length = len(lookupList)
        if length == 0:
            logger.info("Record List is empty. Moving to Next Record")
        else:
            lookupList = random.sample(lookupList, random.randint(1, length))
            logger.info("Inducing {} Error at these offsets".format(len(lookupList)))
            logger.info(lookupList)
            for offset in lookupList:
                logger.info("**** Inducing Error in Record at offsets {}****".format(hex(offset + 8)))
                ReadMetadata(offset + 8)
                EditMetadata(offset + 8)
                ReadMetadata(offset + 8)
                count = count + 1
    return count


# Corrupt Metadata file from random location till end of metadata file
def InduceHugeError():
    count = 0
    with open(filename, 'r+b') as wbfr:
        logger.info("** Corrupting 8byte of Metadata with b'1111222244443333' all place")
        wbfr.seek(-1, os.SEEK_END)
        endOffset = wbfr.tell()
        offset = random.randint(1, endOffset)
        logger.info("Start offset is {}".format(offset))
        while 1:
            offset = offset + 8
            wbfr.seek(offset)
            byte = wbfr.read(8)
            if not byte:
                break
            else:
                EditMetadata(offset + 8)
                count = count + 1
    return count


# Corrupt 512k Metadata in Metadata file from random location
def Induce512kbError():
    count = 0
    j = 0
    with open(filename, 'r+b') as wbfr:
        wbfr.seek(-524400, os.SEEK_END)  # Took a bigger number than 512k
        endOffset = wbfr.tell()
        offset = random.randint(1, endOffset)
        logger.info("Start offset is {}".format(offset))
        while 1:
            offset = offset + 8
            j = j + 8
            wbfr.seek(offset)
            byte = wbfr.read(8)
            if not byte:
                break
            else:
                if j > 524288:
                    break
                else:
                    EditMetadata(offset)
                    count = count + 1
    return count

# This function take stob_f_container, stob_f_key and returns cob_f_cotainer, cob_f_key and device_id
def ConvertAdstob2Cob(stob_f_container, stob_f_key):
    M0_FID_DEVICE_ID_OFFSET = 32
    M0_FID_DEVICE_ID_MASK = 72057589742960640
    M0_FID_TYPE_MASK = 72057594037927935

    # m0_fid_tassume()
    tid = int(67) # Char 'C' Ascii Value
    cob_f_container = ((tid << (64 - 8 )) | (int(stob_f_container, 16) & M0_FID_TYPE_MASK))
    cob_f_key = int(stob_f_key, 16)
    device_id = (int(cob_f_container) & M0_FID_DEVICE_ID_MASK) >> M0_FID_DEVICE_ID_OFFSET

    return cob_f_container, cob_f_key, device_id

# This function take cob_f_cotainer, cob_f_key and returns stob_f_container, stob_f_key
def ConvertCobAdstob(cob_f_container, cob_f_key):
    M0_FID_DEVICE_ID_OFFSET = 32
    M0_FID_DEVICE_ID_MASK = 72057589742960640
    M0_FID_TYPE_MASK = 72057594037927935

    device_id = (int(cob_f_container, 16) & M0_FID_DEVICE_ID_MASK) >> M0_FID_DEVICE_ID_OFFSET
    # m0_fid_tassume()
    tid = 2 # STOB_TYPE_AD = 0x02
    stob_f_container = ((tid << (64 - 8 )) | (int(cob_f_container, 16) & M0_FID_TYPE_MASK))
    stob_f_key = int(cob_f_key, 16)

    return stob_f_container, stob_f_key

# This function corrupt Emap Record specified by Cob Id
def CorruptEmap(recordType, stob_f_container, stob_f_key):
    read_metadata_file()
    lookupList = recordDict[recordType]
    logger.info("Offset List of {} = {} ".format(recordType, lookupList))

    for offset in lookupList:
        print()
        emap_key_data, offset = ReadCompleteRecord(offset)
        if (hex(stob_f_container) in emap_key_data) and (hex(stob_f_key) in emap_key_data) and ("0xffffffffffffffff" not in emap_key_data):
            # 16 bytes of BE_EMAP_KEY (footer) + 16 bytes of BE_EMAP_REC(header) gives offset of corresponding BE_EMAP_REC
            rec_offset = offset + 32
            emap_rec_data, rec_offset = ReadCompleteRecord(rec_offset)
            logger.info("** Metadata at offset {}, BE_EMAP_KEY ek_prefix = {}:{}, ek_offset = {}".format(offset-24,
                        emap_key_data[0], emap_key_data[1], emap_key_data[2]))
            logger.info("** Metadata at offset {}, BE_EMAP_REC er_start = {}, er_value = {}, er_unit_size = {}, er_cs_nob = {}, checksum = {}".format(
                        offset+32, emap_rec_data[0], emap_rec_data[1], emap_rec_data[2], emap_rec_data[3], emap_rec_data[4:],))
            # Check er_cs_nob and if it is not 0 then go and corrupt last checksum 8 bytes
            if emap_rec_data[3] != "0x0":
                EditMetadata(rec_offset-8)

def ListAllEmapPerDevice():
    print("Listing all emap keys and emap records with device id")
    recordType = "BE_EMAP_KEY"
    read_metadata_file()
    lookupList = recordDict[recordType]
    logger.info(lookupList)

    for offset in lookupList:
        print()
        emap_key_data , offset = ReadCompleteRecord(offset)
        stob_f_container_hex = emap_key_data[0]
        stob_f_key_hex = emap_key_data[1]
        cob_f_container, cob_f_key, device_id = ConvertAdstob2Cob(stob_f_container_hex, stob_f_key_hex)
        # 16 bytes of BE_EMAP_KEY (footer) + 16 bytes of BE_EMAP_REC(header) gives offset of Corresponding BE_EMAP_REC
        emap_rec_offset = offset + 32
        emap_rec_data, rec_offset = ReadCompleteRecord(emap_rec_offset)

        logger.info("** Metadata at offset {}, BE_EMAP_KEY ek_prefix = {}:{}, ek_offset = {}, Device ID = {}".format(offset,
                    emap_key_data[0], emap_key_data[1], emap_key_data[2], device_id))
        logger.info("** Metadata at offset {}, BE_EMAP_REC er_start = {}, er_value = {}, er_unit_size = {}, er_cs_nob = {}, checksum = {}".format(
                    emap_rec_offset, emap_rec_data[0], emap_rec_data[1], emap_rec_data[2], emap_rec_data[3], emap_rec_data[4:],))


def VerifyLengthOfRecord(recordDict):
    count = 0
    read_metadata_file()
    logger.info("***********Record list will be print here************")
    for record, items in recordDict.items():
        logger.info(" {} :  {}".format(record, len(items)))
        count = count + 1
    return count


def read_metadata_file():
    with open(filename, "rb") as metadata:
        i: int = 0
        while 1:
            byte = metadata.read(8)
            i = i + 8
            if not byte:
                break
            byte = binascii.hexlify(byte[::-1])
            if byte == header:
                byte = binascii.hexlify((metadata.read(8))[::-1])  # Read the Type Size Version
                rtype, size = ReadTypeSize(byte)
                if rtype not in typeDict.keys():
                    continue
                record = typeDict[rtype]
                i = i + 8
                if size > b'00000000':
                    RecordOffset(record, i, size)
                    # logger.info("*** RECORD TYPE {}, OFFSET {}, SIZE{} ***".format(record, i, size))  #Debug print
                i = int(size, 16) + i - 16
                metadata.seek(i)
            # Not parsing the whole file for few test as It will take many hours, depending on metadata size
            if (args.verify == True) or (args.list_emap == True):
                pass # we will read complete metadata file in case of -v or -list_emap option
            else:
                if i > 111280000:  # Increase this number for reading more location in metadata
                    break


noOfErrs = 0

if args.err512k:
    noOfErrs = Induce512kbError()

elif args.hugeCorruption:
    noOfErrs = InduceHugeError()

elif args.random:
    noOfErrs = InduceRandomCorruption(noOfErr)

elif recordType:
    noOfErrs = InduceCorruption(recordType, noOfErr)

elif args.verify:
    noOfErrs = VerifyLengthOfRecord(recordDict)

elif args.allErr:
    noOfErrs = InduceErrInAllRecord()

elif args.allGMD:
    noOfErrs = InduceErrInGMDRecords()

elif args.allDMD:
    noOfErrs = InduceErrInDMDRecords()

elif args.corrupt_emap:
    _f_container, _f_key = args.corrupt_emap.split(":")
    cob_f_container = hex(int(_f_container, 16))
    cob_f_key = hex(int(_f_key, 16))
    stob_f_container, stob_f_key = ConvertCobAdstob(cob_f_container, cob_f_key)
    CorruptEmap("BE_EMAP_KEY", stob_f_container, stob_f_key)

elif args.list_emap:
    ListAllEmapPerDevice()

if not args.verify:
    logger.info("Number of errors induced by script: {}".format(noOfErrs))

if noOfErrs > 0:
    logger.info("**** Successfully injected holes in metadata ****")
else:
    logger.error("**** Failed to inject holes in metadata ****")
