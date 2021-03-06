/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Pretty-print some well-known BER or DER encoded data (e.g. certificates,
 * keys, pkcs7)
 *
 * $Id$
 */

#include "secutil.h"

#if defined(__sun) && !defined(SVR4)
extern int fprintf(FILE *, char *, ...);
#endif

#include "plgetopt.h"

#include "pk11func.h"
#include "nspr.h"
#include "nss.h"

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -t type [-a] [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s Specify the input type (must be one of %s,\n",
	    "-t type", SEC_CT_PRIVATE_KEY);
    fprintf(stderr, "%-20s %s, %s, %s,\n", "", SEC_CT_PUBLIC_KEY,
	    SEC_CT_CERTIFICATE, SEC_CT_CERTIFICATE_REQUEST);
    fprintf(stderr, "%-20s %s, %s, %s or %s)\n", "", SEC_CT_CERTIFICATE_ID,
            SEC_CT_PKCS7, SEC_CT_CRL, SEC_CT_NAME);
    fprintf(stderr, "%-20s Input is in ascii encoded form (RFC1113)\n",
	    "-a");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    exit(-1);
}

int main(int argc, char **argv)
{
    int rv, ascii;
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    SECItem der, data;
    char *typeTag;
    PLOptState *optstate;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    ascii = 0;
    inFile = 0;
    outFile = 0;
    typeTag = 0;
    optstate = PL_CreateOptState(argc, argv, "at:i:o:");
    while ( PL_GetNextOpt(optstate) == PL_OPT_OK ) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'a':
	    ascii = 1;
	    break;

	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "w");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 't':
	    typeTag = strdup(optstate->value);
	    break;
	}
    }
    PL_DestroyOptState(optstate);
    if (!typeTag) Usage(progName);

    if (!inFile) inFile = PR_STDIN;
    if (!outFile) outFile = stdout;

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: NSS_NoDB_Init failed (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
	exit(1);
    }
    SECU_RegisterDynamicOids();

    rv = SECU_ReadDERFromFile(&der, inFile, ascii);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: SECU_ReadDERFromFile failed\n", progName);
	exit(1);
    }

    /* Data is untyped, using the specified type */
    data.data = der.data;
    data.len = der.len;

    /* Pretty print it */
    if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE) == 0) {
	rv = SECU_PrintSignedData(outFile, &data, "Certificate", 0,
			     SECU_PrintCertificate);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE_ID) == 0) {
        PRBool saveWrapeState = SECU_GetWrapEnabled();
        SECU_EnableWrap(PR_FALSE);
        rv = SECU_PrintSignedContent(outFile, &data, 0, 0,
                                     SECU_PrintDumpDerIssuerAndSerial);
        SECU_EnableWrap(saveWrapeState);
    } else if (PORT_Strcmp(typeTag, SEC_CT_CERTIFICATE_REQUEST) == 0) {
	rv = SECU_PrintSignedData(outFile, &data, "Certificate Request", 0,
			     SECU_PrintCertificateRequest);
    } else if (PORT_Strcmp (typeTag, SEC_CT_CRL) == 0) {
	rv = SECU_PrintSignedData (outFile, &data, "CRL", 0, SECU_PrintCrl);
#ifdef HAVE_EPV_TEMPLATE
    } else if (PORT_Strcmp(typeTag, SEC_CT_PRIVATE_KEY) == 0) {
	rv = SECU_PrintPrivateKey(outFile, &data, "Private Key", 0);
#endif
    } else if (PORT_Strcmp(typeTag, SEC_CT_PUBLIC_KEY) == 0) {
	rv = SECU_PrintSubjectPublicKeyInfo(outFile, &data, "Public Key", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_PKCS7) == 0) {
	rv = SECU_PrintPKCS7ContentInfo(outFile, &data,
					"PKCS #7 Content Info", 0);
    } else if (PORT_Strcmp(typeTag, SEC_CT_NAME) == 0) {
	rv = SECU_PrintDERName(outFile, &data, "Name", 0);
    } else {
	fprintf(stderr, "%s: don't know how to print out '%s' files\n",
		progName, typeTag);
	SECU_PrintAny(outFile, &data, "File contains", 0);
	return -1;
    }

    if (inFile != PR_STDIN)
	PR_Close(inFile);
    PORT_Free(der.data);
    if (rv) {
	fprintf(stderr, "%s: problem converting data (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
    }
    if (NSS_Shutdown() != SECSuccess) {
	fprintf(stderr, "%s: NSS_Shutdown failed (%s)\n",
		progName, SECU_Strerror(PORT_GetError()));
	rv = SECFailure;
    }
    PR_Cleanup();
    return rv;
}
