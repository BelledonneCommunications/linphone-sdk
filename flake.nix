{
  description = "Linphone SIP Software Development Kit (SDK)";

  inputs = {
    bctoolbox-src = {
      url = git+file:./bctoolbox;
      flake = false;
    };
    bcunit-src = {
      url = git+file:./bcunit;
      flake = false;
    };
    belcard-src = {
      url = git+file:./belcard;
      flake = false;
    };
    belle-sip-src = {
      url = git+file:./belle-sip;
      flake = false;
    };
    belr-src = {
      url = git+file:./belr;
      flake = false;
    };
    bzrtp-src = {
      url = git+file:./bzrtp;
      flake = false;
    };
    decaf-src = {
      url = git+file:./external/decaf;
      flake = false;
    };
    lime-src = {
      url = git+file:./lime;
      flake = false;
    };
    liblinphone-src = {
      url = git+file:./liblinphone;
      flake = false;
    };
    mediastreamer2-src = {
      url = git+file:./mediastreamer2;
      flake = false;
    };
    msopenh264-src = {
      url = git+file:./msopenh264;
      flake = false;
    };
    ortp-src = {
      url = git+file:./ortp;
      flake = false;
    };
    soci-src = {
      url = git+file:./external/soci;
      flake = false;
    };
  };

  outputs =
    { self
    , nixpkgs
    , bctoolbox-src
    , bcunit-src
    , belcard-src
    , belle-sip-src
    , belr-src
    , bzrtp-src
    , lime-src
    , liblinphone-src
    , mediastreamer2-src
    , msopenh264-src
    , ortp-src
    , decaf-src
    , soci-src
    , ...
    }:
    let
      version = "sdk-${self.shortRev}";
    in
    {

      devShells.x86_64-linux.default = with nixpkgs.legacyPackages.x86_64-linux;
        mkShell {
          # Build with
          # CC=gcc CXX=g++ cmake -S . -B ./build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Sanitizer -DCMAKE_INSTALL_PREFIX="$PWD/build/install" -DENABLE_UNIT_TESTS=ON -DENABLE_STRICT=OFF -DENABLE_SANITIZER=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=1
          # cd build
          # cmake --build .
          buildInputs = [
            cmake
            clang_13
            (python3.withPackages (ps: with ps; [ pystache six ]))
            pkg-config
            doxygen
            yasm
            libv4l
            xorg.libX11
            libpulseaudio
            glew
            nixpkgs-fmt
          ];
        };

      overlays.default = final: prev: {
        bctoolbox = prev.bctoolbox.overrideAttrs (attrs: {
          inherit version;
          src = bctoolbox-src;
        });
        bcunit = prev.bcunit.overrideAttrs (attrs: {
          inherit version;
          src = bcunit-src;
        });
        bc-decaf = prev.bc-decaf.overrideAttrs (attrs: {
          inherit version;
          src = decaf-src;
        });
        bc-soci = prev.bc-soci.overrideAttrs (attrs: {
          inherit version;
          src = soci-src;
        });
        belcard = prev.belcard.overrideAttrs (attrs: {
          inherit version;
          src = belcard-src;
        });
        belle-sip = prev.belle-sip.overrideAttrs (attrs: {
          inherit version;
          src = belle-sip-src;
        });
        belr = prev.belr.overrideAttrs (attrs: {
          inherit version;
          src = belr-src;
        });
        bzrtp = prev.bzrtp.overrideAttrs (attrs: {
          inherit version;
          src = bzrtp-src;
        });
        lime = prev.lime.overrideAttrs (attrs: {
          inherit version;
          src = lime-src;
        });
        liblinphone = prev.liblinphone.overrideAttrs (attrs: {
          inherit version;
          src = liblinphone-src;
          patches = [ ];
          buildInputs = attrs.buildInputs ++ [
            prev.zxing-cpp
          ];
        });
        mediastreamer = prev.mediastreamer.overrideAttrs (attrs: {
          inherit version;
          src = mediastreamer2-src;
        });
        mediastreamer-openh264 = prev.mediastreamer-openh264.overrideAttrs (attrs: {
          inherit version;
          src = msopenh264-src;
        });
        ortp = prev.ortp.overrideAttrs (attrs: {
          inherit version;
          src = ortp-src;
        });
      };


      packages.x86_64-linux = import nixpkgs {
        system = "x86_64-linux";
        overlays = [
          self.overlays.default
        ];
      };

      defaultPackage.x86_64-linux = self.packages.x86_64-linux.liblinphone;

    };
}
