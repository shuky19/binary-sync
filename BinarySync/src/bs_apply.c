#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "common.h"
#include "bsheader.h"
#include "block.h"

RETURN_CODE parse_args(int argc, char** argv,
					   char** ppDataFilename,
					   char** ppTargetFilename) {

	// Default values
	*ppDataFilename = NULL;
	*ppTargetFilename = NULL;

	printf("Parse arguments\n");

	opterr = 0;
	int c;
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "data", 		required_argument, 0, 'd' },
			{ "target", 	required_argument, 0, 't' },
			{ 0, 0, 0, 0 } };
		c = getopt_long(argc, argv, "d:t:", long_options, &option_index);
		if (c == -1) { break; }
		switch (c) {
		case 'd':
			*ppDataFilename = optarg;
			printf("-- Data file: %s\n", *ppDataFilename);
			break;
		case 't':
			*ppTargetFilename = optarg;
			printf("-- Target: %s\n", *ppTargetFilename);
			break;
		default:
			printf("Invalid option\n");
		}
	}

	CHECK_PTR_RETURN(ppDataFilename, ILLEGAL_ARG);
	CHECK_PTR_RETURN(ppTargetFilename, ILLEGAL_ARG);

	return EXIT_SUCCESS;
}

int checkHeaders(BSHeader* pHeader, FILE* pTagetFile) {
	CHECK_PTR_RETURN(pHeader, ILLEGAL_ARG);
	CHECK_PTR_RETURN(pTagetFile, ILLEGAL_ARG);
	if (pHeader->type != DATA) {
		return -1;
	}
	uint64_t fileSize = getFileSize(pTagetFile);
	if (fileSize != pHeader->totalSize) {
		return -2;
	}
	return 0;
}

uint64_t getBufferSize (BSHeader* pHeader, uint64_t blockId) {
	uint64_t max = getBlockCount(pHeader);
	if (blockId >= max) {
		return 0;
	}
	return (blockId == (max-1)) ? getLastBlockSize(pHeader) : pHeader->blockSize;
}

RETURN_CODE writeBlock(BSHeader* pHeader, uint64_t blockId, uint64_t size, FILE* pTarget, void* pData) {
	CHECK_PTR_RETURN(pHeader, ILLEGAL_ARG);
	CHECK_PTR_RETURN(pTarget, ILLEGAL_ARG);
	if (fseek(pTarget, pHeader->blockSize * blockId, SEEK_SET) != 0) {
		return SEEK_ERROR;
	}
	if (fwrite(pData, size, 1, pTarget) != 1) {
		return WRITE_ERROR;
	}
	return NO_ERROR;
}

RETURN_CODE bs_apply(int argc, char** argv) {

	int rc = 0;
	char* pDataFilename = NULL;
	char* pTargetFilename = NULL;

	FILE* pDataFile = NULL;
	FILE* pTargetFile = NULL;

	BSHeader* pHeader = NULL;

	void* pBuffer = NULL;

TRY

	if ((rc = parse_args(argc, argv,
						 &pDataFilename,
						 &pTargetFilename)) != 0) {
		THROW("Invalid arguments", 1);
	}

	// Open data file
	if ((pDataFile = fopen(pDataFilename, "rb")) == NULL) {
		THROW("Error opening data file", OPEN_ERROR);
	}

	pHeader = readHeader(pDataFile);
	CHECK_PTR_THROW(pHeader, "Error reading header for data file");
	printHeaderInformation(pHeader, TRUE);

	// Open target file
	if ((pTargetFile = fopen(pTargetFilename, "rb+")) == NULL) {
		THROW("Error opening target", OPEN_ERROR);
	}

	// Check
	if ((rc = checkHeaders(pHeader, pTargetFile)) != 0) {
		THROW("Data file is not compatible with target", rc);
	}

	pBuffer = malloc(pHeader->blockSize);
	uint64_t blockId;

	while(!feof(pDataFile)) {
		// Read block Id
		if (fread(&blockId, sizeof(uint64_t), 1, pDataFile) != 1) {
			THROW("Cannot read from data file", READ_ERROR);
		}
		uint64_t dataSize = getBufferSize(pHeader, blockId);
		if (fread(pBuffer, dataSize, 1, pDataFile) != 1) {
			THROW("Cannot read from data file", READ_ERROR);
		}
		printf("Apply data for block %"PRIu64", block size: %"PRIu64"\n", blockId, dataSize);

		if ((rc = writeBlock(pHeader, blockId, dataSize, pTargetFile, pBuffer)) != NO_ERROR) {
			THROW("Error writing block", rc);
		}

		printf("----------------------------\n");
	}

CATCH

FINALLY

	AUTOFREE(pBuffer);
	AUTOFREE(pHeader);
	AUTOCLOSE(pDataFile);
	AUTOCLOSE(pTargetFile);
	return exceptionId;
}

int main(int argc, char** argv) {
	printf("binary-sync: apply\n");
	return bs_apply(argc, argv);
}
