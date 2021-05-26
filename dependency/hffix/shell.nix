# In the nix-shell environment, this derivation should provide everything
# needed for all of the Makefile targets

with import <nixpkgs> {};

stdenv.mkDerivation {
# clangStdenv.mkDerivation { -- TODO clang
  name = "hffix";
  src = ./.;

  buildInputs = [ boost doxygen stack gmp zlib ];
#   buildInputs = [ boost doxygen stack gmp zlib clang]; -- TODO clang
}
