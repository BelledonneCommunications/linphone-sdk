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
      version = "sdk-${if builtins.hasAttr "shortRev" self then self.shortRev else "dev"}";
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

      # CC=gcc CXX=g++ BUILD_DIR_NAME="build-headless" cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B ./$BUILD_DIR_NAME -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Sanitizer -DCMAKE_INSTALL_PREFIX="$PWD/$BUILD_DIR_NAME/install" -DENABLE_UNIT_TESTS=ON -DENABLE_STRICT=OFF -DENABLE_V4L=0
      devShells.x86_64-linux.headless = with nixpkgs.legacyPackages.x86_64-linux;
        mkShell {
          buildInputs = [
            cmake
            clang_13
            (python3.withPackages (ps: with ps; [ pystache six ]))
            pkg-config
            doxygen
            yasm
            libpulseaudio # deleteme
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
          cmakeFlags = attrs.cmakeFlags ++ [
            "-DCMAKE_C_FLAGS=-Wno-error=unused-parameter"
          ];
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

        mediastreamer-headless = final.mediastreamer.overrideAttrs (attrs:
          let
            notIn = list: element: ! builtins.elem element list;
            nameDoesNotStartWith = str: package: with builtins; substring 0 (stringLength str) package.pname != str;
          in
          {
            propagatedBuildInputs = builtins.filter
              (with final; notIn [
                libpulseaudio
                glew
                xorg.libX11
                xorg.libXext
                libv4l
              ])
              attrs.propagatedBuildInputs;

            nativeBuildInputs = builtins.filter (nameDoesNotStartWith "qt") attrs.nativeBuildInputs;

            cmakeFlags = attrs.cmakeFlags ++ [
              "-DENABLE_SOUND=NO"
              "-DENABLE_QT_GL=OFF"
              "-DENABLE_V4L=0"
            ];
            NIX_LDFLAGS = null;
          });

        # WIP/Broken
        linphone-sdk =
          let
            combined-src = prev.runCommand "linphone-sdk-src" { }
              ''
                mkdir $out
                ln -s ${./.}/* $out
                ln -s ${bcunit-src} $out/bcunit
                ln -s ${bctoolbox-src} $out/bctoolbox
              '';
          in
          {
            inherit combined-src;
            all-in-one = {
              sanitizer =
                prev.stdenv.mkDerivation {
                  name = "linphone-sdk-all-in-one";
                  src = combined-src;
                  cmakeFlags = [
                    "-DCMAKE_BUILD_TYPE=Sanitizer"
                    "-DENABLE_UNIT_TESTS=ON"
                    "-DENABLE_STRICT=OFF"
                    "-DENABLE_SANITIZER=ON"
                  ];
                  nativeBuildInputs = with prev; [
                    cmake
                  ];
                };
            };
          };
      };

      overlays.ccached = final: prev:
        let
          withCCache = packages: with builtins; listToAttrs (map
            (name: {
              inherit name; value = (getAttr name prev).override { stdenv = prev.ccacheStdenv; };
            })
            packages);
        in
        {
          ccacheWrapper = prev.ccacheWrapper.override {
            extraConfig = ''
              export CCACHE_COMPRESS=1
              export CCACHE_DIR="/var/cache/ccache"
              export CCACHE_UMASK=007
              if [ ! -d "$CCACHE_DIR" ]; then
                echo "====="
                echo "Directory '$CCACHE_DIR' does not exist"
                echo "Please create it with:"
                echo "  sudo mkdir -m0770 '$CCACHE_DIR'"
                echo "  sudo chown root:nixbld '$CCACHE_DIR'"
                echo "====="
                exit 1
              fi
              if [ ! -w "$CCACHE_DIR" ]; then
                echo "====="
                echo "Directory '$CCACHE_DIR' is not accessible for user $(whoami)"
                echo "Please verify its access permissions"
                echo "====="
                exit 1
              fi
            '';
          };
        } // withCCache [
          "bctoolbox"
          "bcunit"
          "bc-decaf"
          "bc-soci"
          "belcard"
          "belle-sip"
          "belr"
          "bzrtp"
          "lime"
          "liblinphone"
          "mediastreamer"
          "mediastreamer-openh264"
          "ortp"

          "mediastreamer-headless"
        ];


      packages.x86_64-linux = import nixpkgs {
        system = "x86_64-linux";
        overlays = [
          self.overlays.default
        ];
      };

      # ⚠️ Requires ccache configured on the system
      # https://nixos.wiki/wiki/CCache
      # Build packages with ccache like:
      # nix build .#ccachedPackages.x86_64-linux.liblinphone
      ccachedPackages.x86_64-linux = import nixpkgs {
        system = "x86_64-linux";
        overlays = [
          self.overlays.default
          self.overlays.ccached
        ];
      };

      defaultPackage.x86_64-linux = self.packages.x86_64-linux.liblinphone;

    };
}
