# Test build

`w3dproto-patched.3dsx` is built from this branch, which has both fixes:
paged gossip menus (gossip-menu-pages) and auto run on a SELECT tap (auto-run-select).

To send it to a 3DS on the same Wi-Fi:

1. On the 3DS, open the Homebrew Launcher and press Y (netloader).
2. Run `python3 send3ds.py w3dproto-patched.3dsx` (add the 3DS's IP as a second argument if it isn't found).

It's saved as `sdmc:/3ds/w3dproto_test.3dsx`, so your normal install stays as it is.
