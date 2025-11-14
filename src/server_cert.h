// Self-signed SSL certificate for HTTPS web server
// Generated: 2025-11-12
// Valid: 10 years (2025-2035)
// Subject: CN=stationsdata.local, O=VisionMaster, OU=E290
// SANs: stationsdata.local, *.local, localhost, 192.168.4.1, 127.0.0.1
// Key: RSA 2048-bit

#ifndef SERVER_CERT_H
#define SERVER_CERT_H

// Server certificate (PEM format)
// Valid for mDNS (stationsdata.local) and AP mode (192.168.4.1)
const char server_cert_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIEADCCAuigAwIBAgIURyCiM7K8hpIRgQTxto+Ofo/pB8QwDQYJKoZIhvcNAQEL\n"
"BQAwbzELMAkGA1UEBhMCVVMxDjAMBgNVBAgMBVN0YXRlMQ0wCwYDVQQHDARDaXR5\n"
"MRUwEwYDVQQKDAxWaXNpb25NYXN0ZXIxDTALBgNVBAsMBEUyOTAxGzAZBgNVBAMM\n"
"EnN0YXRpb25zZGF0YS5sb2NhbDAeFw0yNTExMTIxMjI4MThaFw0zNTExMTAxMjI4\n"
"MThaMG8xCzAJBgNVBAYTAlVTMQ4wDAYDVQQIDAVTdGF0ZTENMAsGA1UEBwwEQ2l0\n"
"eTEVMBMGA1UECgwMVmlzaW9uTWFzdGVyMQ0wCwYDVQQLDARFMjkwMRswGQYDVQQD\n"
"DBJzdGF0aW9uc2RhdGEubG9jYWwwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n"
"AoIBAQDPUtxjbl1T8y5ufnysDklXs6K8DQhuV/+eUz/SWJ2qiVIgcpNIBoevN62L\n"
"9b5TduUQq2IXha+ZWmMGgith8hco/C5s+MA78yux+3n+xm0tBtZTVFUIvQGarKGX\n"
"pOLFK2vb4x5moQV8M+FAekbDIbTkQVHAaLp8dOUnijOdiIF8vvr50RuWTKY9XUgp\n"
"FCSd62n6M14j1+uxt7ebEr8FVrJfcp8mH562xuJlEHFo+Teiv6rB5RruQoaDaLZx\n"
"AT37bIkCpJy9SsIqiYhrm6CyZ37qQRW8TV+VtoEucYD+szqfkYk+J8nf/3uxth8q\n"
"Yo5O4STgXGJOxE9B95fzx8nBg6RDAgMBAAGjgZMwgZAwHQYDVR0OBBYEFLn0w6cv\n"
"V/h8NvIdnxDg2AkoMtLiMB8GA1UdIwQYMBaAFLn0w6cvV/h8NvIdnxDg2AkoMtLi\n"
"MA8GA1UdEwEB/wQFMAMBAf8wPQYDVR0RBDYwNIISc3RhdGlvbnNkYXRhLmxvY2Fs\n"
"ggcqLmxvY2Fsgglsb2NhbGhvc3SHBMCoBAGHBH8AAAEwDQYJKoZIhvcNAQELBQAD\n"
"ggEBALis5X2YldgLxfLi5O3HqMfItU4Cy/Y8n2rAbMRhSlMlDD8iQtSK51xyspS7\n"
"1Yc8pVl1mE9CYgDflbxZElY09mV8hiGGyjeNlqld1i01+mtLo6qkmwdgKWm7xFz3\n"
"t7h50kIJkPkvTO3+xCjVJyY8yKMslQew7PX7H4fFhuOCD/iBd4tmXPztdgoTlopp\n"
"ZBtL8Wb6gGpeCV0Hy0RCMXzXl2hpECT13egj1w6tiPXcPfSdzPFCgbKDGXo0EAgu\n"
"nF6zqpBNABzAom2v9lK5/vcX+oOsjExn1QduR6HTFczgzRuhB7bBnJ9XcxEFKrie\n"
"YhC5J224C6f/TTQ1qzPvT3oxMJw=\n"
"-----END CERTIFICATE-----\n"
;

// Server private key (PEM format)
const char server_key_pem[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEuwIBADANBgkqhkiG9w0BAQEFAASCBKUwggShAgEAAoIBAQDPUtxjbl1T8y5u\n"
"fnysDklXs6K8DQhuV/+eUz/SWJ2qiVIgcpNIBoevN62L9b5TduUQq2IXha+ZWmMG\n"
"gith8hco/C5s+MA78yux+3n+xm0tBtZTVFUIvQGarKGXpOLFK2vb4x5moQV8M+FA\n"
"ekbDIbTkQVHAaLp8dOUnijOdiIF8vvr50RuWTKY9XUgpFCSd62n6M14j1+uxt7eb\n"
"Er8FVrJfcp8mH562xuJlEHFo+Teiv6rB5RruQoaDaLZxAT37bIkCpJy9SsIqiYhr\n"
"m6CyZ37qQRW8TV+VtoEucYD+szqfkYk+J8nf/3uxth8qYo5O4STgXGJOxE9B95fz\n"
"x8nBg6RDAgMBAAECggEAVM5YSHQEySpYe+pRpS/S4IUitDnAkSJ99Y2oNar6E6BI\n"
"b+1uQbAqIIpt+ypyyEGCQedZILYWtmw6xZbJmC8nOiSt+PVn7R3zacKsjIMudZSu\n"
"Zze/8OsflN243Acem/i/DhtBfEmxrLSsF2vtNjS7ggMWJdaxMee/NQVbR71m+leI\n"
"XHm56v4sU3uXdhzJe0Id56XH2HoUxKwgs/KH1x43O8ShT3FNlrwYAStNoubt6rkD\n"
"E/bJOyulfoWtzMEWnLTr3CQ2924pzdgOwNupDCvWUC8O4zD0pAO6Gd730Qe5shil\n"
"NBb+CSRMlcU4UFZ9wk3SveX945jhIzST+kBaBGbwwQKBgQDrgilkg2B3GnGFiYZP\n"
"XU8anu9CmnSlwtMiY8YNkfg0peQhScVguzk+0yp40IGboZw7tq0o5hzyIigoC7dV\n"
"MhhstOoYoYH+nEQ748r6DCI3Y9BEX2CCUDdY+1rdsnDGUlkbbjObWzVWV6Am5eC9\n"
"qgnc89TsZgwcHdEoh4Hzmz0pjQKBgQDhXOWcSmJzQ9kXe2DqpIBDCkCFlve8NGdl\n"
"d+Yif+qdkAmEAoZtRVW60fZUvsYiv+g/oZ9pRBIEIA6NdKedLSSp7hTJDvouAyMu\n"
"zdinfc+bcepfDe8oZZRClC0oh+20wv/TrZzD8j3UbgH4+n3cUyPu9ulweEcW/SJR\n"
"VZ5WcqlJDwKBgQCe3anwfNMo5PSpQRESHn6LFaWOh0SiwN7ONwHWC56kXTeb4Pi9\n"
"fO5r2+StpPGZO5Z5jYwXp8rk53exM94TIXzqb8vum5xmVaGNyOcWb/Lw7GsEhFZE\n"
"8bm0U7KDFKkQj2I+p8M2THuZZ/jH5JALQVXv76e1ZE1M9iwuq+JUo2bJzQKBgGi9\n"
"Xy3LWIRPxUbnnbyQJdiCnEg6SPtcs61yEzB3mRgPyIxlDAsfDWAdk0oBvF6MKKni\n"
"OQ+YhnMKXxkZXYlsYLzlnR1w64+U/7YSD56Ql3ucbxwsgrmYtFZZPb+3pR+8/V9p\n"
"MhHTtS7Uze/ko7hRn8LBWO9fx1KE5X09uLBe7BS7An9ix1rUZjKecpugSY3wucnq\n"
"7cSvsZOTJF64r/pUnC/ehfq/2IsscDxO5/7aUCgQkUGzXwdTeeLEHWHoHuQWAQ80\n"
"jijHSp+RERcNF/B92Wv8wCdUiQs1BVXtbzUmQSyrcXoJHa3vLzofzDfwhoUAIXdg\n"
"EgiMTB52SSZWe6i72zo2\n"
"-----END PRIVATE KEY-----\n"
;

#endif // SERVER_CERT_H
