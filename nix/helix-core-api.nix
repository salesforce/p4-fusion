{ fetchzip }:
{
  "1.1" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl1.1.1.tgz";
      hash = "sha256-0jU51a239Ul5hicoCYhlzc6CmFXXWqlHEv2CsTYarS0=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl1.1.1.tgz";
      hash = "sha256-a+4dovGVvqH1Kyon8m8j319iYlDRriU2BraCbxCgL0U=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl1.1.1.tgz";
      hash = "sha256-OfVxND14LpgujJTl9WfhsHdPsZ/INd9iDw5DcyzglLU=";
    };
  };
  "3.0" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl3.tgz";
      hash = "sha256-cTSy1uy6IxhFqOD3ZSkbj6aDXizg8MuiHQoi4wOsngo=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl3.tgz";
      hash = "sha256-KEUVsc6fwQEA45ShG3rbaVEY4gDmS5GB+RCwcL5bIiU=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://filehost.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl3.tgz";
      hash = "sha256-1iPHQXGSnelLJRo8RdTVDEpb80G1bcNsLWuvf4aHW+g=";
    };
  };
}
