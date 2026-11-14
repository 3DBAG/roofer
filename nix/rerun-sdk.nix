{
  stdenv,
  fetchurl,
  unzip,
  cmake,
  arrow-cpp,
}:
stdenv.mkDerivation {
  pname = "rerun-sdk";
  version = "0.27.3";
  src = fetchurl {
    url = "https://github.com/rerun-io/rerun/releases/download/0.27.3/rerun_cpp_sdk.zip";
    sha256 = "1aysn1jsl58vxaakv8j9awnnxll06ay4pwczfs8gi63w3w4yj920";
  };
  nativeBuildInputs = [
    unzip
    cmake
  ];
  buildInputs = [ arrow-cpp ];
  cmakeFlags = [ "-DRERUN_DOWNLOAD_AND_BUILD_ARROW=OFF" ];
}
