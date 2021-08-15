class Clippy < Formula
  desc "Making clipboard work over SSH"
  homepage "https://bitpowder.com:2443/bvgastel/clippy"
  #url "https://bitpowder.com:2443/bvgastel/clippy/-/archive/0.1.1/clippy-0.1.1.tar.gz"
  #sha256 "2777e5f5b4f19f93913e2e97187ccc71b61825a85efc48cb0358b2d2e3cca239"
  head "https://bitpowder.com:2443/bvgastel/clippy.git", branch: "main"
  license "BSD-2-Clause"

  depends_on "bsdmake" => :build
  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build", "--target", "install"
  end

  test do
    system "#{bin}/clippy"
  end
end
