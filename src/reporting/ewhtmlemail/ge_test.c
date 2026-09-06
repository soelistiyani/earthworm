
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b64.h"
#include "hmac.h"


int main(int argc, char **argv) {

unsigned char * decoded_secret;
unsigned char * url_to_sign;
unsigned char digest[25];
char * signature_encoded;
int bytes_decoded, bytes_url;

if (argc <= 1) {
   printf("ge_test secret url\n");
   exit(1);
}

url_to_sign = (unsigned char *) argv[2];
bytes_url = (int)strlen((const char *) url_to_sign);
decoded_secret = b64_decode (argv[1], strlen(argv[1]));
bytes_decoded = (int)strlen((const char *) decoded_secret);


printf("decoded_secret: %d bytes in string, %s\n", bytes_decoded, decoded_secret);
printf("url: %d bytes in string, %s\n", bytes_url, url_to_sign);

hmac_sha1( decoded_secret, bytes_decoded, url_to_sign, bytes_url, digest);
printf("digest: %zu bytes in string, %s\n", strlen((const char *)digest), digest);
signature_encoded = b64_encode(digest, strlen((const char *)digest));
printf("signature: %zu bytes in string, %s\n", strlen(signature_encoded), signature_encoded);

return 0;
}
