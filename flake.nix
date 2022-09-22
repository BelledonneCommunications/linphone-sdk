{
  description = "Linphone Software Development Kit (SDK)";

  outputs = { self, nixpkgs }: {

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

    packages.x86_64-linux.hello = nixpkgs.legacyPackages.x86_64-linux.hello;

    defaultPackage.x86_64-linux = self.packages.x86_64-linux.hello;

  };
}
