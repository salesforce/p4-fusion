{ fetchzip }:
{
  "1.1" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl1.1.1.tgz";
      hash = "sha256-gblxNxyvjGHmrE54178JN4NTvI2EW3a58Yj7KngEr1o=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl1.1.1.tgz";
      hash = "sha256-9H0qbYU9yFcdqooLvXSkqYIiYIX9d+BaSh9dlMo1D5Q=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl1.1.1.tgz";
      hash = "sha256-SBeHjVuodRlLmAYA7/5qSASmneKaa7acXvhEPTxQd/w=";
    };
  };
  "3.0" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl3.tgz";
      hash = "sha256-s75750zXUxLbx3Bs2kjtGTKI1sKHO+tAQTF2d0dc234=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl3.tgz";
      hash = "sha256-uCR91Kbidq8DyJSw+Dpmiyn66LIkveNHZFwCfbBZ6dk=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl3.tgz";
      hash = "sha256-JaFjUjJP1GQ4iFCtuWbhYNonH3PiNlgI0z2L6UsN1JE=";
    };
  };
}
