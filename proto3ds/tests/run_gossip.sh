#!/bin/bash
# PC check of the gossip window's pages (gossip_page_test.cpp): game_ui*.cpp against the libctru stub, no recorded session.
set -e
P=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-/tmp/w3d_ui}
mkdir -p "$OUT"
N="$P/../net"
NET="$N/game.cpp $N/game_social.cpp $N/game_spells.cpp $N/game_items.cpp $N/game_life.cpp $N/factions.cpp $N/game_factions.cpp \
     $N/game_vendor.cpp $N/game_quests.cpp $N/game_group.cpp $N/game_pet.cpp $N/game_friends.cpp $N/game_services.cpp $N/game_mail.cpp \
     $N/game_auction.cpp $N/game_trade.cpp $N/game_water.cpp $N/tables.cpp $N/result_text.cpp $N/objects.cpp $N/transport.cpp $N/movement.cpp $N/client.cpp \
     $N/crypto.cpp $N/socket.cpp $N/inflate.cpp"
UI="$P/source/game_ui.cpp $P/source/game_ui_widgets.cpp $P/source/game_ui_tabs.cpp $P/source/game_ui_windows.cpp $P/source/game_ui_keyboard.cpp \
    $P/source/ui_style.cpp $P/source/hud.cpp $P/source/ui_assets.cpp $P/source/pad.cpp $P/source/action_layout.cpp \
    $P/source/questwatch.cpp $P/source/questdb.cpp"
INC="-I$P/tests/ui_stub -I$N -I$P/source -I$P/../common -I$P/tests"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -Wall $INC "$P/tests/gossip_page_test.cpp" $UI $NET -o "$OUT/gossip_page_test"
"$OUT/gossip_page_test"
