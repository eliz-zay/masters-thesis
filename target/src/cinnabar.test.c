#include <math.h>
#include <string.h>
#include <stdio.h>
#include "cinnabar.c"

static char signature[] =   "5b23bfbe96c5a0c28508399cf3d9c38d12d6cc5b06c949ff6e5d59c481a433ca"
                            "521912ad15f021360322cc94b054fa6381792363544823ff114695de5a895a13"
                            "fa9d59e469d53a325ac70fdfea6dbd4b4df0d1652aea50436682f95b07c113d0"
                            "20dc4c3aa39e2fb775bc0b32990cd906c48dcd8235ba7298246da3b2c8f4fc6d"
                            "e7683fd5e74f90fdc2bd073fb2ace9135287f4ebd1917ce365ee05d646fcb0c9"
                            "ef0eb90507b59495a35c66d51aafc91473e517846a171c460081827a63d44c7c"
                            "7849e0bacc87373aedc37d8fb17badcd86f80ef74e21501e73d401ddd97830da"
                            "f0cbbc03b5dec3dc2b565742e0e36684b4570598c15a4f83b7c200bd93edb561";

static char privatekey[] =  "-----BEGIN RSA PRIVATE KEY-----\n"
                            "MIIEpAIBAAKCAQEAq9Sl9byoS1h4UmemYVUc31Jd9p0CyFZXgLVhhTm8lLq98v51\n"
                            "uOO7JY4ClKp8WmZ/Hv1RDkxlYtqvrF3zJNgWjgJogQHiV4TdjXtr+CC+7QN9qkTI\n"
                            "k9AWdT1ocL/wzAL6lloqDXYWob3Dl8A3ylrH6qAAumh6GMpq6XwbGM+jr4kLGm4P\n"
                            "VOH30WbVwTqUreyRQJatzscx2gWGeCai80SFaNxAjD/9oMGp+pogSEKVlq1o2dyy\n"
                            "IwSSViDtd/sG2/t2tCBFTkY6PgDLOWQRQX/A34oUPjpShbw4KVxAXHDVhB+0f7X+\n"
                            "kLq5+lbDk4RnZPU3kGCexzeSAEb+4SalABeTIwIDAQABAoIBAA6DyAEaDp1Ou9s6\n"
                            "JjPSnL3Al29dk/6YTIvyxmoalnN50tHT7N3RXt2tQUqNnDOGtPZJL6+lhGr1TiGh\n"
                            "TgiuuDkGuw0qu5PpBU4OPvCW04nx4Yugg9D4ou0EYu4jSJPzLHfG5gZ9EyxWe082\n"
                            "TYAqavjGy0jzylyNvLo8YY2W/Jy3M18vcIZkYbncXWOIDEutVnoXratoJpDcp9VX\n"
                            "Zmjbn5ij2HU5nRXdzBoqkLCyOKLpESubKe0oGv7+Do9eTj9fr0U5RDOFQAPoXjPi\n"
                            "zOHi5H2oTqM9M40ZhHcG81Xw2J+xv6svkv0INChCJt/v6njm8y8be6J8VchqOr15\n"
                            "ugNhbgECgYEA3Ct+jlAVHKHyf8x5VecDj4BHT3/3XnjksG306GYW3/JWHx0cALjd\n"
                            "jhQCcADJM3xvAdwaM3nuU1Z8mPaMefR+Vb0jW97jF7AvnsFVvJ7VonGFDaf5t75i\n"
                            "eEc+HawyDYR5JVa1/+zJyfyuAlkECB25JRUrFgKsOgJBzxWnU+d/dHsCgYEAx8tK\n"
                            "he2jDBlYn/MGD/30vtFppTGv62D6syxDrrOO644KhOYqij9vHcCM1ANFKOTksAYR\n"
                            "aBRGs5J11J/pe0EcRECU5GeNJRcMRFGJHarQTEb0LgG6XxySpfZOfRkdjYXUdiVV\n"
                            "59xlC1hfi9w43KadLZOv27qOqSju2CHbdL6J/3kCgYAi/78EfHJ+tLfJ3QVExI5q\n"
                            "V2f+mUcHe4xPB4uxDdmBDBLoq0XyT3DYzxF8IIPbbWJwFz8LA80A7nSsFDVMhbM3\n"
                            "ifN+/TV4ZIeNYwpwC4fGZOlTvGoT7W3V1O1o5iCmyXJAn0IbRtblBwfaU7AyYhc2\n"
                            "b+EDhLVAG2++raCF0/0M1QKBgQCG20U2GSzQ4drcO+F/sd8dXaR9iIhBzHfrsJkO\n"
                            "tsxlWr7m7aURI7gQ0QM9p+dqrvVdivr80ZLXaqh2GGo0c8Jsn1rgwLSYsHHrO03d\n"
                            "5IosskfnNetif5rMwvA/qFA2UnsSNClEE5NwkPoNIVyQMzYsqV8uZUIeFC8DW/cR\n"
                            "WfszoQKBgQCThl++bj+rdlid/Kg4+E3twMnCBvhFxz6R4m3AJRUwRMBFzOnmUv9l\n"
                            "j+CBJEi33tHJ1XPc21eSJu4GTMgYWtTwQ+xxrcY2XxHyCsjRHBb1532Ex2UtQ13e\n"
                            "W5MWrtKGrSdy+yKwDZ/iKDLO7aFpwJ9RbzmJIP02mG2osIUPBq9OHw==\n"
                            "-----END RSA PRIVATE KEY-----\n";

static char publickey[] =   "-----BEGIN PUBLIC KEY-----\n"
                            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAq9Sl9byoS1h4UmemYVUc\n"
                            "31Jd9p0CyFZXgLVhhTm8lLq98v51uOO7JY4ClKp8WmZ/Hv1RDkxlYtqvrF3zJNgW\n"
                            "jgJogQHiV4TdjXtr+CC+7QN9qkTIk9AWdT1ocL/wzAL6lloqDXYWob3Dl8A3ylrH\n"
                            "6qAAumh6GMpq6XwbGM+jr4kLGm4PVOH30WbVwTqUreyRQJatzscx2gWGeCai80SF\n"
                            "aNxAjD/9oMGp+pogSEKVlq1o2dyyIwSSViDtd/sG2/t2tCBFTkY6PgDLOWQRQX/A\n"
                            "34oUPjpShbw4KVxAXHDVhB+0f7X+kLq5+lbDk4RnZPU3kGCexzeSAEb+4SalABeT\n"
                            "IwIDAQAB\n"
                            "-----END PUBLIC KEY-----\n";

static int test_signature(const char * message, const char * signature) {
    CNBR_SIGNATURE sign;

    CnbrStatus status = CnbrSignature(&sign, message, strlen(message), privatekey);
    if (status != CnbrSuccess) {
        printf("failed: signature returned %d\n", (int) status);
        return 1;
    }
    
    if (strlen(signature) != 2 * sign.signature_length) {
        printf("failed: signature length = %d, expected %d\n", (int) sign.signature_length, (int) strlen(signature) / 2);
        return 1;
    }

    static char hexa[] = "0123456789abcdef";
    for (size_t i = 0; i < sign.signature_length; i++) {
        if (hexa[(sign.signature_data[i] >> 4) & 0x0F] != signature[2 * i] ||
            hexa[sign.signature_data[i] & 0x0F] != signature[2 * i + 1]) {
                printf("failed: wrong signature\n");
                return 1;
        }
    }

    status = CnbrVerifySignature(sign.signature_data, sign.signature_length, message, strlen(message), publickey);
    if (status != CnbrSuccess) {
        printf("failed: check signature returned %d\n", (int) status);
        return 1;
    }

    CnbrEraseSignature(&sign);
    return 0;
}

int main(int argc, char *argv[]) {
  test_signature("test", signature);
  return 0;
}