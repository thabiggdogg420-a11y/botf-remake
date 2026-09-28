// Content scenarios (Lane G): original catalogue text and the minor-race roster.
//
// 1. IP hygiene: no description in the reference catalogue (or the minor roster) may contain a sentence
//    of the original game's text. The original sentences are represented only by 64-bit FNV-1a hashes of
//    their normalized form (lower-case alphanumerics, single spaces), so no original prose ships here.
// 2. Minor races: roster integrity, seeded placement scaled to galaxy size, join bonuses that vanish on
//    secession, progression, temperament, home defense and the optional MINOR_RACES save section.
#include "MinorRaces.h"
#include "ReferenceCatalogue.h"
#include "ShipRoster.h"
#include "Simulation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
int failed = 0;
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try {
        fn(); ++passed; std::cout << "PASS " << name << std::endl;
    } catch (const std::exception& error) {
        ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    }
}

// Hashes of every sentence (and every whole description) of the original catalogue prose, normalized.
constexpr std::array<std::uint64_t, 939> OriginalSentenceHashes{{
    0x003637ba50236903ULL, 0x0088f09ba0fcc5d6ULL, 0x00be58f4b6fe12f8ULL, 0x012bd6d5d30a1df6ULL,
    0x016703f5490d2f46ULL, 0x017c370efa44b677ULL, 0x01c313044ee171e4ULL, 0x01ed97a602b636a5ULL,
    0x023fdbf54fe3f1d5ULL, 0x02412f849d5f6c8bULL, 0x027f71044724e6dcULL, 0x0298777da0197d4bULL,
    0x02a10e3c2a6e6ca1ULL, 0x034c05c1be641820ULL, 0x03952e112d7099abULL, 0x03b9d4369b7d9b32ULL,
    0x03e45c028e6fa2d2ULL, 0x045b9f685289b793ULL, 0x04aecb3a462a6200ULL, 0x04f561a422b4c354ULL,
    0x0502688472b7400cULL, 0x055bee6ddfa4b254ULL, 0x05c3728e88c69618ULL, 0x0611a76aeae55decULL,
    0x06309aced7e4979bULL, 0x0643d6a9b3eea7fcULL, 0x0677f62e0ae09271ULL, 0x0681ff64d89adf4eULL,
    0x068bfcd546270f50ULL, 0x06b922cb8e5b7720ULL, 0x06cf20abe6d7acfbULL, 0x0757534e3819dcd7ULL,
    0x07be1d8712cadc95ULL, 0x08311a4047cc8513ULL, 0x084623d034a809abULL, 0x085bbc46b95b05a0ULL,
    0x085f099b77878fc5ULL, 0x087ff31c0aa2c47bULL, 0x098237a29136e90fULL, 0x0983685f41053bf1ULL,
    0x09b70decb18919a2ULL, 0x09ced08adecde703ULL, 0x0a038b98ae080777ULL, 0x0a14cb3895747e10ULL,
    0x0a5f95e41cd9fef5ULL, 0x0a6dfe48ab10a79bULL, 0x0a7233f9d586dc62ULL, 0x0ab54c9e62217c07ULL,
    0x0ac806c50713733aULL, 0x0ae814d9c24ebc36ULL, 0x0b0bf1acffc5ebbaULL, 0x0b5ee17351a2f086ULL,
    0x0b6ea08ab0d03d6fULL, 0x0b710521f97a9cbeULL, 0x0bc47d836155dfdbULL, 0x0c5145b3f74d490fULL,
    0x0c523e7a08bc9aa3ULL, 0x0cda6715e2fc4d19ULL, 0x0d68ebe1e0f61cfcULL, 0x0d6a2e53404c1307ULL,
    0x0dc6817ef6822aaaULL, 0x0dcec311a3fd9602ULL, 0x0dd138b99128cc37ULL, 0x0de29426b2976af6ULL,
    0x0dff8d300645300eULL, 0x0e88291e0f2ca0c8ULL, 0x0ebbbe13874f1239ULL, 0x0f29c9ec5494f1e3ULL,
    0x0f691f17445f7da6ULL, 0x0f6b97d2862d5aedULL, 0x0f7e1d606d83b4a2ULL, 0x0fd7f26ce62dcc1cULL,
    0x0ff017c855e189eaULL, 0x0ffe512d29bb0a51ULL, 0x0fff42cf2fdf8fbdULL, 0x10346000691b6b36ULL,
    0x106943b10ff7773bULL, 0x107c82852257e02dULL, 0x108926c12c9d05f7ULL, 0x10b3ad270582f2d0ULL,
    0x10e28b14ce791b57ULL, 0x1144fc6f7c4bdc56ULL, 0x11944a9b1084886cULL, 0x11a7833b16cf164bULL,
    0x11afb9c74af818ddULL, 0x11d4028858b0b545ULL, 0x1253d2e092e791a0ULL, 0x12c0285a3c020257ULL,
    0x12c4b4aa3c4c0a58ULL, 0x13625cb86d34b5b3ULL, 0x14006d34f49b0f89ULL, 0x140990b98b93c43dULL,
    0x140ee3584b2a2134ULL, 0x148d4eb5cb1cf1caULL, 0x14b5cf43e77aba9bULL, 0x14d858d6c1cfec60ULL,
    0x14fee160ec84c9efULL, 0x1526a5856808e378ULL, 0x154995c81cce2b8eULL, 0x157074ef9add4868ULL,
    0x15aa9ad545c3c992ULL, 0x15abbaaeec4cea6dULL, 0x1640ff8afe5f447bULL, 0x1667c8e7a8141af4ULL,
    0x1702243e7e37290fULL, 0x1771a92f47ca54d2ULL, 0x17eec1c1a6ccb4f2ULL, 0x183c70404b160177ULL,
    0x1843b89a391bdc55ULL, 0x18b78a996d18b362ULL, 0x1933d47a4d7da92dULL, 0x199cc255c4322da7ULL,
    0x19ab1b86419356dfULL, 0x1a00f54d9ef81de1ULL, 0x1a410a7b081f0a19ULL, 0x1a41becc142fd0e7ULL,
    0x1a4d258135ddf97bULL, 0x1b20880ffceca7c4ULL, 0x1b20d1841b59bfa5ULL, 0x1b5cf8d0564269d0ULL,
    0x1b8b282f85ca7600ULL, 0x1bd2e87707ebd75dULL, 0x1c2949b54f747509ULL, 0x1c2a7c8b289d1364ULL,
    0x1c32639134c0b83eULL, 0x1cc2f3da7f22c4dcULL, 0x1cceb73206983678ULL, 0x1cef80ae2d796299ULL,
    0x1d08962e4d3f7091ULL, 0x1d6e86d8359dd62cULL, 0x1d6f2b95d1a19ea3ULL, 0x1df352fadf175f75ULL,
    0x1e20a43504c1fd3fULL, 0x1e39117332ac41fbULL, 0x1e5a13ee67e4526eULL, 0x1e5b29cbe1061540ULL,
    0x1e60660fe9f637efULL, 0x1eb9874c41ca01f3ULL, 0x1ef509a61033a460ULL, 0x1f14ae1237dd7f8fULL,
    0x1f2bf3fc8abca514ULL, 0x1ff7835084bf36deULL, 0x202707b7bdfcf8bdULL, 0x20307d78ba3db505ULL,
    0x20b52e3fd76f125eULL, 0x20c38e1a1287144dULL, 0x212c14e040e741beULL, 0x214c237b061070feULL,
    0x2170e3489413b986ULL, 0x21c5b48efa1be4eaULL, 0x221c7f4d221c1273ULL, 0x22568fd8c79116f3ULL,
    0x22acec603651712cULL, 0x2302646c0d46a4e7ULL, 0x236d5a0fd3b0c68cULL, 0x23988d253d89690bULL,
    0x23b2f8f6d3f14eebULL, 0x23fa9620748ac0cdULL, 0x241b3593a47c7acfULL, 0x24cb5d81d517139aULL,
    0x25674ebd9201ad7bULL, 0x2570e4119f81a905ULL, 0x25a72de139bdaf8eULL, 0x2684c9cf5e4ceecaULL,
    0x269e7e213164be77ULL, 0x26d22bf16bc9a5eaULL, 0x26e0b36f74aef601ULL, 0x26efa1536ce01ad1ULL,
    0x2722f11fc59d2fc0ULL, 0x27509f5e4213e748ULL, 0x27e37c41ccb66334ULL, 0x282fe1f9c743c0ddULL,
    0x286256bad026be12ULL, 0x2870ec9cb25885e6ULL, 0x28c1831ad952ceedULL, 0x28c4064501cdd74cULL,
    0x2979b4a9c54a8453ULL, 0x2a778e2a7ae86996ULL, 0x2ad2698f1b87ba70ULL, 0x2b12b809d2b03158ULL,
    0x2b1371ee23d33bbcULL, 0x2b44f5dbeb11100eULL, 0x2bad5349c8096ff6ULL, 0x2cafe2791c2de361ULL,
    0x2cfeebefecd52d7bULL, 0x2d077f8696eb4bd2ULL, 0x2d7587011a9a57e5ULL, 0x2e4a502e2104549fULL,
    0x2e68ec8e2a81bd5cULL, 0x2e9647960d4c8d20ULL, 0x2eddbcfa5047b009ULL, 0x2ef0ee1be01cf924ULL,
    0x2f0d408a393479f5ULL, 0x2f0eb9378a94d5b2ULL, 0x2f522283cc09da17ULL, 0x2f6a96218cd162adULL,
    0x2f8325e1bb12da32ULL, 0x2f960d8528787cf7ULL, 0x3013e3aa23a59b0aULL, 0x30338321ce454a37ULL,
    0x30482e55bf3df207ULL, 0x30a2361ef332b8d0ULL, 0x30c1e07a80d55b14ULL, 0x31bf1c3334986b85ULL,
    0x31d9366520226a3bULL, 0x32033495dcf08c6cULL, 0x3220d21abbaff6e4ULL, 0x326247a896a9616bULL,
    0x328570b9900ef7feULL, 0x335423ce980b09c3ULL, 0x336b75a011bb9923ULL, 0x3390eae40f20b44eULL,
    0x339f4b49db83be8aULL, 0x33c2816477e85a79ULL, 0x34f0792b8cfdb1acULL, 0x3531a88b7aa47990ULL,
    0x3533dfb8adea1714ULL, 0x3538b2b4db602dd5ULL, 0x35682453a535a60eULL, 0x357ba5b8a22d0f40ULL,
    0x35d2c2c89ac386aeULL, 0x36112857b707af67ULL, 0x36133ee31d8530a0ULL, 0x367575edfe9dd02bULL,
    0x367cd64f0234c57dULL, 0x36aad5dddc0f77deULL, 0x36c27e1425971249ULL, 0x37518ecc349890ddULL,
    0x375af2f61efe8bdaULL, 0x37e2fe9a25d3048bULL, 0x38292ed91281b16eULL, 0x383157cfb1a63847ULL,
    0x385296bb2aed2c66ULL, 0x39a3c5399f5eebeaULL, 0x39dc9a86af3a9b0aULL, 0x39e6a02aa53b49b9ULL,
    0x3a296c71210bdb30ULL, 0x3a351f34a3536426ULL, 0x3a6dd8c38051e0f5ULL, 0x3b8895d85abfd75dULL,
    0x3bd4daedb7f5af77ULL, 0x3c8ff1389593f739ULL, 0x3d5891b06f895376ULL, 0x3dfa50b0ef5707b3ULL,
    0x3e1471297e6c6dd8ULL, 0x3e53dfb8eac65b9aULL, 0x3ecf68babd9c6ce4ULL, 0x3f188607523f0f29ULL,
    0x3f5740b57ca179f1ULL, 0x3f8257e7856d2133ULL, 0x3f9d2c1f8eb27d0aULL, 0x3fb7cf9931431067ULL,
    0x400f0adfbb4819d5ULL, 0x40b837015d4bc1deULL, 0x40bea7d2f11614b7ULL, 0x40e4f7906793c2c3ULL,
    0x4128b41d70c9cb43ULL, 0x414510e8496b4e43ULL, 0x417a3d09d5e49cf3ULL, 0x41fc8ec076091c18ULL,
    0x4276db38c6c37a48ULL, 0x428bc8d688af0ec4ULL, 0x42af78b5a319d07fULL, 0x42b4179795f170d2ULL,
    0x42d0952a49ea3eceULL, 0x42ed54223af3eb88ULL, 0x42ff904131ed5925ULL, 0x434990a3c67f67d0ULL,
    0x43536a903f69df08ULL, 0x4356f3a1809d6a8eULL, 0x43f23be8a4b8ddccULL, 0x44a2b5ae65ac02ccULL,
    0x450dadff2f7b21c8ULL, 0x45602dd83f06c610ULL, 0x45a8e894af989cf0ULL, 0x462d559f0d69c22aULL,
    0x466e8aced0a4f4fdULL, 0x46dd228450a4fc8eULL, 0x46e466ba96a2dd03ULL, 0x4722aead3208c529ULL,
    0x47577b413b1aab19ULL, 0x476e2227a80530ebULL, 0x4814a9302e842893ULL, 0x48164c8a96f9eae8ULL,
    0x4817ff157c1a34daULL, 0x4853c74240e2fc0cULL, 0x488a8f9ed8a7e775ULL, 0x49833e21fd1766c7ULL,
    0x4a6c34b9905f18aaULL, 0x4a8b45aafefe7458ULL, 0x4aba373943b233b5ULL, 0x4acdc45da6de6986ULL,
    0x4b848801888be509ULL, 0x4bc7e8b864993da9ULL, 0x4bdfb36ee51ae8c6ULL, 0x4c07756a229ba75cULL,
    0x4c48547f76497a43ULL, 0x4c86489cbb7e702eULL, 0x4d2a6597874892d2ULL, 0x4d3d83fa24a2bee1ULL,
    0x4d73a93729cc1e93ULL, 0x4dd23fb360ff0b08ULL, 0x4dd7b35b4f056c9aULL, 0x4e6045ed95afe7b8ULL,
    0x4e67c3a4cee94304ULL, 0x4ec39b7f063d465eULL, 0x4edcb602093e2b21ULL, 0x4f03ca3d742e35e2ULL,
    0x4f2bec68e8fffbcaULL, 0x4f815f04e83c6682ULL, 0x4fd5a92f265305e6ULL, 0x503311de771a9c93ULL,
    0x506285fdf86492adULL, 0x507b0a32d8c1bad5ULL, 0x50bd8ddbcaf4774aULL, 0x5132361fe140eacdULL,
    0x513b4093f2787b63ULL, 0x51e597545b2f9633ULL, 0x52bc8963fae2949aULL, 0x5308886d6a084f2dULL,
    0x53b170d8036b405eULL, 0x53d6ebce0013335fULL, 0x544665f0f98b4b3dULL, 0x54b0fb6d7df525d1ULL,
    0x54c0ec8b89128488ULL, 0x552310b302b21194ULL, 0x552674355ef7f3feULL, 0x5553af311e11c4a6ULL,
    0x55e6e21ae7b952cdULL, 0x5670192e361368e8ULL, 0x5684e74293dc92fdULL, 0x56e3f293ee8e5eafULL,
    0x571eff70dd0e53c2ULL, 0x57230e0bd40b14bdULL, 0x57309ab2db1d8d58ULL, 0x576cdcb975db4714ULL,
    0x57976f5404ea7eb9ULL, 0x57c9ece839bc5c09ULL, 0x585e78ef140c8e48ULL, 0x586ae295246b6f4dULL,
    0x589d4d658a79ff4bULL, 0x59c0561afe314561ULL, 0x59ce46ce25e411f6ULL, 0x5a18cbcc5d69d9ddULL,
    0x5ac0fd11b4c6f937ULL, 0x5b08bb73d62ca61cULL, 0x5b9d9f1dcd11aaabULL, 0x5baabdf79aecff87ULL,
    0x5bbc69a948e00405ULL, 0x5bbded428a53dda2ULL, 0x5bd9871f798ba79dULL, 0x5bfba79052796bd2ULL,
    0x5cd8ab7594f888e7ULL, 0x5d12b73c2439484eULL, 0x5d2e33ab63031136ULL, 0x5d4a1d17e9acf043ULL,
    0x5d54a6695f1f6f82ULL, 0x5db67930b9ce8617ULL, 0x5e2ccac0b95caefeULL, 0x5ec64d682f7a4154ULL,
    0x5ed895b62e11a51fULL, 0x5fb8caa4e88f0fc4ULL, 0x6038688d5b308752ULL, 0x6052350dc679f4a3ULL,
    0x606b7a21c51aa686ULL, 0x61b1895a3de59db9ULL, 0x62690bd41b8dd916ULL, 0x62e4d92f676833e5ULL,
    0x62fe6ed28d300d7dULL, 0x6309384de0b301a8ULL, 0x634d018a8302c6a5ULL, 0x63d7cafac6cc03ffULL,
    0x647326ddb208b6c9ULL, 0x64f13183d9ad3379ULL, 0x64f2f1ef397a05c6ULL, 0x65b78345d0f2767bULL,
    0x65c28642b036d04fULL, 0x65c4dee876aef47eULL, 0x65fb8c3c99534c3aULL, 0x661e06957bd99024ULL,
    0x662ab900a3f8a11dULL, 0x666a74d8f084f3a7ULL, 0x666d644c9b7ed8d7ULL, 0x66d63a1db289742bULL,
    0x67294cb277ca7ae5ULL, 0x678f31dd4fd611baULL, 0x685d2966a715238eULL, 0x68765bea765b942fULL,
    0x6882173be3de04faULL, 0x68ac6341007ff3eeULL, 0x68bce36290647a50ULL, 0x68d7b89b42a02155ULL,
    0x68da9a2ef886ca61ULL, 0x6923e85c8154f2f1ULL, 0x692c0d9f1ad1e82aULL, 0x6982b64c2d0024a5ULL,
    0x6986c491977d479bULL, 0x699c5a644f017e8cULL, 0x699e7fb94208d5ddULL, 0x69e97b0646d35600ULL,
    0x6a475151fda5d072ULL, 0x6a60e7b88ac2b16fULL, 0x6a6767d053d9ee57ULL, 0x6a9ac850b1bb5ab2ULL,
    0x6b60106d97f74d2eULL, 0x6b7aa47ced114c0eULL, 0x6c330abf260bb777ULL, 0x6c66e86511bdd1eeULL,
    0x6c9e247e2a461a19ULL, 0x6cac5ffc688edc74ULL, 0x6cb141760a275c5cULL, 0x6cd430e5ae124030ULL,
    0x6cdab9adeb8008c6ULL, 0x6cf72b6de1cdf065ULL, 0x6d23df54d4225e28ULL, 0x6d38bd834f60bbf4ULL,
    0x6d5c7b019dca00b5ULL, 0x6d8d73fd82ee807fULL, 0x6dba2c6502251948ULL, 0x6e76afe66df59821ULL,
    0x6f9c32b26de84c4bULL, 0x6fa07109b402fcd3ULL, 0x7023a4f068a8d5aeULL, 0x705a9ebf6c576c8aULL,
    0x7097d7981e7d764aULL, 0x70fbaf0cb9a53e2eULL, 0x715fec8a16b072e4ULL, 0x71816d48930f93a1ULL,
    0x718c5a0dfdaa4ea6ULL, 0x719c3ca1ecc2b17dULL, 0x71f35cb14823c76aULL, 0x72294e95f287b22eULL,
    0x72308ff146e6e00fULL, 0x7264087f69a00c2aULL, 0x72af38a307cd26b8ULL, 0x73271d9bc413447aULL,
    0x733bca8d12b97c6fULL, 0x733c3d0d1a3408faULL, 0x734d906d0c18c38cULL, 0x73834fa8f7f67b76ULL,
    0x73bdd4b6262e82bbULL, 0x742f04a23ce51ffaULL, 0x74ceb312a13d732fULL, 0x759c8b85d4b57a67ULL,
    0x75a4ed406f10df8fULL, 0x75bca85d91856b7eULL, 0x76567379085d76a5ULL, 0x76aa3371146a65e9ULL,
    0x76c7f7a1fbd95cafULL, 0x76d6e2c74e999a75ULL, 0x76ddf4a3b7bdc9dbULL, 0x771237f9fae099a4ULL,
    0x7718d8e580e660beULL, 0x7747642322199732ULL, 0x774a45c0e9471e9bULL, 0x774bb1a93f0210f2ULL,
    0x77ac679e14152380ULL, 0x7843b416e81796a1ULL, 0x78859e1340551194ULL, 0x78a8f98adc25da52ULL,
    0x78f278a714d67004ULL, 0x799b15325ef59c35ULL, 0x7a7a4b55ab6076e8ULL, 0x7a7c7981eec5b87dULL,
    0x7aaa509a71b4d0ceULL, 0x7ab793e686f19c7fULL, 0x7ade4e6c9eaedf11ULL, 0x7afd09fdfe7e31fcULL,
    0x7b201c4db5356c51ULL, 0x7b33b44962857ee1ULL, 0x7b5e9d2bd0d1ad1eULL, 0x7b77dc4c2cd8b792ULL,
    0x7bbb1211e4b63263ULL, 0x7bdb366699aee444ULL, 0x7c23925efcab1036ULL, 0x7d9ec2c3c50cef3dULL,
    0x7e9a86e780a90b95ULL, 0x7ef00c8dc237052cULL, 0x7f485b0192c182b6ULL, 0x7f81f7ca0dd9d654ULL,
    0x7fa87cd410a56192ULL, 0x7faba78cc22925c8ULL, 0x7fd88337c2c50a46ULL, 0x807654aa27aca25fULL,
    0x80786cd20d1a2a10ULL, 0x80b6ebb9cbe2cf33ULL, 0x80db56a9840a4b90ULL, 0x810a5f84c15e54caULL,
    0x8113a643517de60eULL, 0x811e8e7030a40273ULL, 0x8159b8522e053e3bULL, 0x815a3cee0f08a3b1ULL,
    0x81c90b703bc0837cULL, 0x81cf146f34a4b4cfULL, 0x822a36d07b172bf0ULL, 0x8232e00be89082acULL,
    0x82594ca6bb3a8822ULL, 0x82bd2fd78f21f91aULL, 0x831dd80d78f1de58ULL, 0x8337c08b773d3d63ULL,
    0x833babd8e731c2d0ULL, 0x833c261f240a22d2ULL, 0x833f0c31a66b054cULL, 0x836f4a848a2f9aa1ULL,
    0x8375dabc484b9f0dULL, 0x844faff78e73ad49ULL, 0x85465dbe9429f7ddULL, 0x866eb2470e87ac3eULL,
    0x871d210be56f2f70ULL, 0x873f2415e5fbc617ULL, 0x87f7cd5d4e5f80c5ULL, 0x883e8e886da2ee23ULL,
    0x88eb374d16e658dfULL, 0x89016ea86930b34eULL, 0x897a88c6016da1deULL, 0x89a2d46d43e02c24ULL,
    0x89b4b830dca1384fULL, 0x89cf9da42a31f31fULL, 0x89e18ad0b49504a2ULL, 0x89f6caf111b15498ULL,
    0x8a05ec5564ac928fULL, 0x8a0f72a31c3d580dULL, 0x8ad2dc7170281c70ULL, 0x8ae748829f449fe2ULL,
    0x8b18909ed5b778aeULL, 0x8b2d5660033d39d4ULL, 0x8b74eb7507554379ULL, 0x8bfad0ad675e7865ULL,
    0x8c01e9523ee8ef93ULL, 0x8c08442788658929ULL, 0x8c1066647f679a39ULL, 0x8cc448cb4ab4d17cULL,
    0x8cf6d2205bb699f4ULL, 0x8d034f1ef519170cULL, 0x8d47685f0abb457eULL, 0x8d6ad1ee7eacb424ULL,
    0x8da0abb5dbfb3a70ULL, 0x8e7d0c2312206bf7ULL, 0x8e8f718e9cfefd9dULL, 0x8ea2b4f421a4ba86ULL,
    0x8f024bf1ad6b506bULL, 0x8fa47e88b3473ff2ULL, 0x8fae4257efe9c1f5ULL, 0x8fb63610fbba08cfULL,
    0x8fb71c598a717beaULL, 0x8ff1b04aef4bc8cfULL, 0x9013a02d4e129a30ULL, 0x902cf279003350e5ULL,
    0x90d9637856c9d578ULL, 0x90ff14e767c1e3e9ULL, 0x9102ebef3254aecaULL, 0x9144beba45d35d08ULL,
    0x916194301a77cce3ULL, 0x91e061c44bddbe25ULL, 0x91ec2ffd2a04d7e3ULL, 0x9202af82017c35b8ULL,
    0x920ec247612bfac0ULL, 0x921f6ffe9123bb2aULL, 0x923397bbfdad1e53ULL, 0x925a1b2163e40547ULL,
    0x9263978f97713a92ULL, 0x9373f73314de6eb0ULL, 0x93847a5f526da93bULL, 0x93a50e49fd189c9fULL,
    0x93b9bb3996f5f5c7ULL, 0x946a243816dfdf97ULL, 0x947395162de57f42ULL, 0x9475fcc143b0345fULL,
    0x953fbb8b6336b97aULL, 0x9553ae758173d207ULL, 0x95545fe9c3d3e09dULL, 0x95b141df5c5ff985ULL,
    0x95d354a31319f2d3ULL, 0x9627ab0fbe1c20f5ULL, 0x963e6f9a929acbb0ULL, 0x967e83e59e861e4fULL,
    0x968ddec660127afcULL, 0x96dc0431f4ee0518ULL, 0x974e1b33d8a4523cULL, 0x97f94c6484cbc72dULL,
    0x988e65b235469e11ULL, 0x98b93cde6825cc30ULL, 0x98c3af90dc198947ULL, 0x98e8b410d7458596ULL,
    0x99268a17166d5d90ULL, 0x9a02b5308f409d46ULL, 0x9a0992cf623ac7feULL, 0x9abd25e4ccc8b8ccULL,
    0x9af09f38e6dee088ULL, 0x9afe459eca4b7d54ULL, 0x9b17ca592025bcf8ULL, 0x9b87319a3292794aULL,
    0x9c2e1b1dc280b0baULL, 0x9caf8cbf27beea98ULL, 0x9d32788cd6fe9819ULL, 0x9d5df98ae6ab6acbULL,
    0x9e510d8b5789afeaULL, 0x9e7c24a718b83fd6ULL, 0x9f6ebda828fc87f1ULL, 0x9f72fb55756b0776ULL,
    0x9f94d72b7c944206ULL, 0x9fda47c413f85e92ULL, 0xa01d404aad035666ULL, 0xa03ee540eb425627ULL,
    0xa0517394b4d8de6cULL, 0xa0decf4677555cddULL, 0xa10f237b3ada50bfULL, 0xa11a1c8a33d38825ULL,
    0xa1bf8d35054760dbULL, 0xa20857ff3437bda8ULL, 0xa2425b25ca4d7455ULL, 0xa252bb32f283bc34ULL,
    0xa2667c45963bc54eULL, 0xa29eddb11e12d45eULL, 0xa2c51c470f3431d2ULL, 0xa3045d34bdccaabeULL,
    0xa326a5fd2d6edd3cULL, 0xa32c52fdada99404ULL, 0xa363310a0316f314ULL, 0xa363b30be0d78643ULL,
    0xa38d8d4e33667266ULL, 0xa43602d2d9766010ULL, 0xa4535e4a850b43eaULL, 0xa45c48d110a06559ULL,
    0xa4a643038f02708fULL, 0xa4b945beb016373fULL, 0xa4bb7b6b855447f0ULL, 0xa4cfa4eee96642a2ULL,
    0xa51e26d49f5edafdULL, 0xa55d2637ab22b72fULL, 0xa589036c9c781e84ULL, 0xa5cda6813cb6bc22ULL,
    0xa5e69bb8cccbda87ULL, 0xa65be86e94a8297aULL, 0xa66dde5a3b2d442bULL, 0xa6920795a729852aULL,
    0xa69351f167d9dfe0ULL, 0xa6f829f48b97c503ULL, 0xa703f97a11a79190ULL, 0xa79f777f767c5f94ULL,
    0xa8d7348a8feedf96ULL, 0xa8e65041d7711fb1ULL, 0xa9526a2d1a916b31ULL, 0xa9c17e4ac6b6194eULL,
    0xa9d12d4a73de7ea3ULL, 0xaa049363d4c7469dULL, 0xaa28633b28472075ULL, 0xaa6f2dc95e6d2384ULL,
    0xab644f1d80b96de8ULL, 0xabcaa54b4a599867ULL, 0xac23cd5f2c801527ULL, 0xac790f65ff81a23dULL,
    0xacb6606e795b1e69ULL, 0xacbea578aca996eaULL, 0xad6151e0225dbd00ULL, 0xadc15a35cdafad36ULL,
    0xadc559da23178c70ULL, 0xae0582ac124f806fULL, 0xae09b303eea358d1ULL, 0xae8cb03a1664e695ULL,
    0xaeb18cc4afa97bb8ULL, 0xaf265257124e99a9ULL, 0xaf64318c3f64270eULL, 0xaf7d2c98c6d65574ULL,
    0xaf99d03e3981a1eeULL, 0xaf9eff76a2ff72d7ULL, 0xafe794758f6780baULL, 0xaffe2e72ce755989ULL,
    0xb039fea376c2b4a1ULL, 0xb08b32e2abcdbff6ULL, 0xb0f2d442bde26a13ULL, 0xb10b1dc2a8bdb975ULL,
    0xb1109a6a64124758ULL, 0xb15a97f5f81dcb50ULL, 0xb16e22b0171d6e73ULL, 0xb295586d626048efULL,
    0xb2aee24e2f7d3c77ULL, 0xb2e86dce470f0820ULL, 0xb2e9f12fa53fc60aULL, 0xb327683cdd4ac830ULL,
    0xb32874d7c2268b31ULL, 0xb42876a8a0b63f96ULL, 0xb42cc8efe8ee8b89ULL, 0xb4df795e8f474578ULL,
    0xb51ede442f7abfb7ULL, 0xb5ec8d8c757dbcc0ULL, 0xb61c3fa6d4356074ULL, 0xb64d9298e8aa8ac7ULL,
    0xb6d91f4158ee417dULL, 0xb77232f4b9110f71ULL, 0xb7a60cb8bb0458c2ULL, 0xb7b803766b3b72baULL,
    0xb7e3da2d1911b0d9ULL, 0xb80a15c3e1644b2fULL, 0xb814835f98659ad7ULL, 0xb87f05d270c8a599ULL,
    0xb882976a44cbc3cdULL, 0xb95ec15a3a19f10cULL, 0xba3b89ac8bc5b98fULL, 0xbaa2928060ae6a25ULL,
    0xbaa9cfe52c9ae06fULL, 0xbaad5d9cb55cf9edULL, 0xbabea8a9ce098f37ULL, 0xbad3b1b76d2841e6ULL,
    0xbb03ee0cd6dccf79ULL, 0xbbc58378e6c3f448ULL, 0xbbec6dffaaa8b352ULL, 0xbd841fe74527e05aULL,
    0xbe3cfeac7acf69eeULL, 0xbe53c54671cf4b0cULL, 0xbe984b3a8d5338abULL, 0xbeb597baf73d621aULL,
    0xbeedf410838a56a5ULL, 0xbef792a619fc59c3ULL, 0xbf6f5325048a2ccfULL, 0xbf8969208584ecf7ULL,
    0xbfff00393272adafULL, 0xc009f5cb374ced00ULL, 0xc0339c618b563e5bULL, 0xc041decee25ed064ULL,
    0xc072c70c3c304976ULL, 0xc08d839b63c9b7afULL, 0xc1a6ac92b128fe3eULL, 0xc1b51ef15a30a877ULL,
    0xc1bd23e9923341c3ULL, 0xc276dfdb7c3e0171ULL, 0xc2ddca79b3dc0731ULL, 0xc3ea907f44734d7aULL,
    0xc48bc1dc535ca134ULL, 0xc4b1d74500fbc0e6ULL, 0xc564d8f0275e00f5ULL, 0xc5bff33c14e456d8ULL,
    0xc5d6ebc93a023cfeULL, 0xc679b16a69759711ULL, 0xc67cd3d98586b3e4ULL, 0xc68eb1e6a0031149ULL,
    0xc6a8f41849b63852ULL, 0xc7ccef30b11dd927ULL, 0xc8b5c926198a0f0dULL, 0xc903545edbe641a7ULL,
    0xc9604a5c2c8d1a8bULL, 0xc99bd358d4009289ULL, 0xc9b4ae6e92036a0bULL, 0xca69ebca458b791cULL,
    0xca6b21018c552874ULL, 0xca743311a2662881ULL, 0xca97345c5f5056bfULL, 0xca9f3825565a9dc8ULL,
    0xcae852bf6f804beaULL, 0xcb14b910d6883f2fULL, 0xcb1f95ca1930959dULL, 0xcb2bff9aad7c995bULL,
    0xcbdbb8615ff76ce3ULL, 0xcc2686d14f36de29ULL, 0xcc30a8610cd30c65ULL, 0xcc943f092cdb350cULL,
    0xcc9c08cb9bed0d5cULL, 0xccd54d95bac85b14ULL, 0xcced73d4a2b489d3ULL, 0xccfdbd5b3eda4f26ULL,
    0xcd1caad75ac646ecULL, 0xcd4220b927eb99b9ULL, 0xcdbaf5e121bd2075ULL, 0xcdce544eb8604f4cULL,
    0xcde96c89754bbb5bULL, 0xce5340ade49a9f39ULL, 0xce79f60a4b72bbeaULL, 0xce9b578b6e7167c4ULL,
    0xcf5895cf74d3c2a6ULL, 0xcf845627c78f7b56ULL, 0xcf9d3544baaf38beULL, 0xd0037a07b07d556aULL,
    0xd01c2f266f87261eULL, 0xd0347dadc370741dULL, 0xd128fd7198083474ULL, 0xd20dd3e907f653c5ULL,
    0xd25ced9089b95bd6ULL, 0xd262de8e7af677b4ULL, 0xd26d19df9926c05aULL, 0xd28055e7bde0baa8ULL,
    0xd2b48b36a7a8aaa2ULL, 0xd2bed93657eae4c6ULL, 0xd2d1f6da5369b734ULL, 0xd3036f4c48d2279dULL,
    0xd3baae93ad6b6d7fULL, 0xd419cae7f1bb0f9fULL, 0xd42237068d2d66feULL, 0xd438bb835b798bc2ULL,
    0xd4472730cce904d2ULL, 0xd4687abccb444637ULL, 0xd4dc99d144797578ULL, 0xd4e6f3fb14b57474ULL,
    0xd5f10cf8fc59f0fbULL, 0xd62c95d6be901eceULL, 0xd70bbae048b1e0d0ULL, 0xd73ae362e4b61121ULL,
    0xd766886a90ff52a9ULL, 0xd771a5f2eb53652fULL, 0xd7fdc9d7e624efa1ULL, 0xd8010153a5484e9eULL,
    0xd848059dc0441809ULL, 0xd8bf435779284de6ULL, 0xd8efc14bde08e573ULL, 0xd934c14867fd0b20ULL,
    0xda5d835a8ca09e54ULL, 0xdb0af5f1eb8ffe2fULL, 0xdb81c13e198647c5ULL, 0xdb95690cf594dd1cULL,
    0xdb9cc132098e7137ULL, 0xdcdeecf5d238c772ULL, 0xdcf3b089ff37464aULL, 0xdd3453191989268cULL,
    0xdd814d8de70cd129ULL, 0xddc0fe43890287adULL, 0xde71ed762cd6c294ULL, 0xdef614d5d941bf0eULL,
    0xdf23a78cc94627b7ULL, 0xdfb2669abe538874ULL, 0xdfcb2d6e0c0a2418ULL, 0xdfff711bd2a714c2ULL,
    0xe056f310947c741fULL, 0xe06290c6cabf9761ULL, 0xe128700687bd240bULL, 0xe1d568aeb46750bcULL,
    0xe2638176fa2cf52eULL, 0xe2f0fbb3cd7533a2ULL, 0xe3329aa9b724ff03ULL, 0xe3c14ba04c113984ULL,
    0xe3d41670c43dc59eULL, 0xe3f07fc0bfe4bd38ULL, 0xe47b14e820615d47ULL, 0xe48227d78ccbfd46ULL,
    0xe4a6864ef7383b23ULL, 0xe4e607a2af783ae3ULL, 0xe4fa67e0eb3a99eaULL, 0xe538fec2700db85aULL,
    0xe57a85b436fc5e03ULL, 0xe5dc7df433c514a7ULL, 0xe6170f3a6e66b94fULL, 0xe6389fc98085a9daULL,
    0xe68b677fc78d08a2ULL, 0xe699011abcb1f6ccULL, 0xe6a25af37241c6e2ULL, 0xe6f36f6e16f03330ULL,
    0xe6f87e11835af6fdULL, 0xe78173033e79917bULL, 0xe7a72acdb4a58f3cULL, 0xe7d72ae50df54dd9ULL,
    0xe8530b45082831c9ULL, 0xe858bc20eb1f0f2cULL, 0xe867020a2f02fd8fULL, 0xe8f932feb97deaf2ULL,
    0xe950386916a29992ULL, 0xea02483e2e5bb1eaULL, 0xea41f3859bb8d1e0ULL, 0xeaab5f4035d24700ULL,
    0xeb5b220e7f604e17ULL, 0xeb5ef184513abe72ULL, 0xeb7ebb0ef9cfe20fULL, 0xeb84da5e8f1090e8ULL,
    0xebabb4bdb806c460ULL, 0xebb8e3954ea37db3ULL, 0xebdeea79fbdb5e67ULL, 0xebe355759a9cc9eaULL,
    0xec878ac7f35618b5ULL, 0xeccc208ed51bec25ULL, 0xed0416492935a2acULL, 0xed933d17ab98a8acULL,
    0xedc0cdbe0a993b39ULL, 0xede0418949c112dcULL, 0xedef06740362268aULL, 0xee1585091db8e906ULL,
    0xef6e2ecf4c3cff5bULL, 0xef97cd05ecf726f1ULL, 0xefb700a78e4ea313ULL, 0xf0019a7fcc957370ULL,
    0xf0311d63ce56e2c5ULL, 0xf0622a02819a13f0ULL, 0xf10d82bf5eb2a02aULL, 0xf12663402b6e731aULL,
    0xf1326734dc6218e6ULL, 0xf142701e989ae9c5ULL, 0xf16fd5e918dfab0cULL, 0xf1a342595bf80121ULL,
    0xf1bcc66c7f97a7d9ULL, 0xf1c960cdd4876feaULL, 0xf1d791b6f1232d6dULL, 0xf230356c43f6da4dULL,
    0xf2401cd497c930beULL, 0xf2bc5c60535a5d2cULL, 0xf2d0cd62c5fe5e34ULL, 0xf3511244d7bd751aULL,
    0xf35ba63125366a53ULL, 0xf4a29e74abaad24bULL, 0xf4c417d8c51f6a4eULL, 0xf506e5b69ffb74bfULL,
    0xf53f6e85e75301e5ULL, 0xf547acdeab044d34ULL, 0xf57623f896999022ULL, 0xf58886d2fb353198ULL,
    0xf601aa5d71f2b7ebULL, 0xf68173d6708be1beULL, 0xf6a9248b13cdd4b5ULL, 0xf6aff6094d6cd14aULL,
    0xf79aee10db962f25ULL, 0xf7b17cbcf2962c47ULL, 0xf7e317c7948922ebULL, 0xf81df9de13fa2a52ULL,
    0xf81dfc36a43113ecULL, 0xf86cf1153ec5c9e0ULL, 0xf92058fc2a3b0b04ULL, 0xf941522f4faf9c1eULL,
    0xf9625d11fc70dc06ULL, 0xf9b1cfc034846573ULL, 0xf9ff72964d04f4ddULL, 0xfa215023987d781dULL,
    0xfa67ef4dcd935156ULL, 0xfae13e14d45d4668ULL, 0xfaebbd7738ec0650ULL, 0xfb107c51933e3e79ULL,
    0xfb2a3bc1e258b8c6ULL, 0xfb43a4b962bc6154ULL, 0xfb552b18ab7dd356ULL, 0xfbf72021954dcbd2ULL,
    0xfc35fcd6d718a92eULL, 0xfd10e2aa9be832e7ULL, 0xfd23efe1d2ad5305ULL, 0xfd2688d250837e2cULL,
    0xfd5e3470adda352fULL, 0xfd7701a56f52a495ULL, 0xfdb44b9bbf16b910ULL, 0xfe071248ca1f64e5ULL,
    0xfe8cc08f3387ea8aULL, 0xfeb9679058515da4ULL, 0xff21cf2a8bb5ab6bULL, 0xff47574420f03e66ULL,
    0xff702a90a1f7895fULL, 0xff817759e771d248ULL, 0xffe95e97584aeb08ULL,
}};

std::string normalize(const std::string& text) {
    std::string out;
    bool space = false;
    for (unsigned char ch : text) {
        const bool alnum = ch < 128 && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'));
        if (alnum) {
            if (space && !out.empty()) out.push_back(' ');
            space = false;
            out.push_back(static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch));
        } else {
            space = true;
        }
    }
    return out;
}
std::uint64_t fnv(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : text) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash;
}
std::vector<std::string> sentences(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        current.push_back(text[i]);
        const char ch = text[i];
        if ((ch == '.' || ch == '!' || ch == '?') && (i + 1 == text.size() || text[i + 1] == ' ' || text[i + 1] == '\n' || text[i + 1] == '\t')) {
            out.push_back(current); current.clear();
        }
    }
    if (!normalize(current).empty()) out.push_back(current);
    return out;
}
bool originalHash(std::uint64_t h) {
    return std::binary_search(OriginalSentenceHashes.begin(), OriginalSentenceHashes.end(), h);
}
// Known markers of the extracted original data (short fragments only).
constexpr std::array<const char*, 6> OriginalMarkers{{
    "Biotech represents", "DO NOT TRANSLATE", "tech at 0th level", "sythesize", "Rule #", "\\u000b",
}};

void checkOriginalProse(const char* description, const std::string& what) {
    check(description != nullptr, what + ": missing description");
    const std::string text = description;
    check(text.size() >= 40 && text.size() <= 400, what + ": description length out of range");
    for (unsigned char ch : text) check(ch >= 32 && ch < 127, what + ": description must be printable ASCII");
    check(text.back() == '.', what + ": description must end a sentence");
    for (const char* marker : OriginalMarkers) check(text.find(marker) == std::string::npos, what + ": original-text marker present");
    const auto whole = normalize(text);
    check(!originalHash(fnv(whole)), what + ": description matches the original text");
    for (const auto& sentence : sentences(text)) {
        const auto n = normalize(sentence);
        check(n.size() < 20 || !originalHash(fnv(n)), what + ": sentence copied from the original text: " + sentence);
    }
}

botf::Colony& colonyOf(botf::GameState& s, int systemId) {
    for (auto& c : s.colonies) if (c.systemId == systemId) return c;
    throw std::runtime_error("missing colony");
}
botf::EmpireState& empireOf(botf::GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    for (auto& r : s.rivals) if (r.id == id) return r;
    throw std::runtime_error("missing empire");
}
const botf::System& systemOf(const botf::GameState& s, int id) {
    for (const auto& x : s.systems) if (x.id == id) return x;
    throw std::runtime_error("missing system");
}
int distance(const botf::System& a, const botf::System& b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

// Gives minor `m` the identity of roster race `raceId` (test setup only).
void becomeRace(botf::GameState& s, botf::Minor& m, int raceId) {
    for (const auto& other : s.minors) check(&other == &m || other.raceId != raceId, "race already present");
    const auto* r = botf::minors::raceById(raceId);
    m.name = r->name;
    botf::minors::adopt(m, *r, botf::StartingEra::Early);
    if (auto* ms = s.diplomacyEngineState.getMinorState(m.systemId)) ms->raceName = r->name;
}
const botf::minors::MinorRaceRecord* firstRace(const std::function<bool(const botf::minors::MinorRaceRecord&)>& pred) {
    for (const auto& r : botf::minors::Roster) if (pred(r)) return &r;
    throw std::runtime_error("no roster race matches");
}
// Makes `m` a member of `owner` with a joined colony, as the membership pipeline would.
void join(botf::GameState& s, botf::Minor& m, int owner) {
    m.contact[static_cast<std::size_t>(owner)] = true;
    m.memberOf = owner;
    botf::Colony colony;
    colony.systemId = m.systemId; colony.ownerId = owner;
    colony.population = 30; colony.foodWorkers = 15; colony.industryWorkers = 10; colony.researchWorkers = 5;
    botf::minors::configureJoinedColony(m, colony);
    s.colonies.push_back(colony);
    std::sort(s.colonies.begin(), s.colonies.end(), [](const auto& a, const auto& b) { return a.systemId < b.systemId; });
    const auto& sys = systemOf(s, m.systemId);
    auto& known = empireOf(s, owner).explored;
    known.push_back(sys.y * s.width + sys.x);
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
    botf::syncDiplomacyState(s);
}
// A new campaign as clients start one (newCampaign places the minor-race roster).
botf::GameState fresh(std::uint64_t seed, int empires = 5, bool reference = true,
                      botf::RulesetPreset preset = botf::RulesetPreset::Classic, int w = 16, int h = 12, int systems = 28,
                      botf::StartingEra era = botf::StartingEra::Early) {
    botf::CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.reference = reference; setup.preset = preset;
    setup.width = w; setup.height = h; setup.systems = systems; setup.era = era;
    return botf::newCampaign(setup);
}
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    return body + "CHECKSUM " + std::to_string(fnv(body)) + "\n";
}
} // namespace

int main() {
    int passed = 0;
    using botf::minors::JoinBonus;
    using botf::minors::Temperament;

    run("normalizer and hash agree with the offline hash generator", [] {
        check(normalize("  The Quick-brown FOX, jumps over 2 lazy dogs!  ") == "the quick brown fox jumps over 2 lazy dogs", "normalize");
        check(fnv(normalize("  The Quick-brown FOX, jumps over 2 lazy dogs!  ")) == 0x4ccc64077ac13a2cULL, "fnv");
        check(std::is_sorted(OriginalSentenceHashes.begin(), OriginalSentenceHashes.end()) && OriginalSentenceHashes.size() > 500,
              "hash table sorted and complete");
    }, passed);

    run("technology descriptions are original remaster text", [] {
        std::set<std::string> seen;
        for (const auto& t : botf::referenceData::TechnologyCatalogue) {
            checkOriginalProse(t.description, std::string("tech ") + t.name);
            check(seen.insert(t.description).second, std::string("duplicate tech description: ") + t.name);
        }
    }, passed);

    run("ship descriptions are original remaster text", [] {
        for (const auto& ship : botf::referenceData::ShipCatalogue)
            checkOriginalProse(ship.description, std::string("ship ") + std::to_string(ship.id) + " " + ship.shipClass);
        // Expanded roster (SHIP-ROSTER.md): same originality rules, and no two classes share a description.
        std::set<std::string> seen;
        for (const auto& c : botf::roster::ExpandedRoster) {
            checkOriginalProse(c.description, std::string("roster ship ") + std::to_string(c.id) + " " + c.className);
            check(seen.insert(c.description).second, std::string("duplicate roster description: ") + c.className);
        }
    }, passed);

    run("building descriptions are original remaster text", [] {
        for (const auto& b : botf::referenceData::BuildingCatalogue)
            checkOriginalProse(b.description, std::string("building ") + std::to_string(b.id) + " " + b.name);
    }, passed);

    run("minor roster: at least 24 races, unique ids, names, homeworlds and valid references", [] {
        const auto& roster = botf::minors::Roster;
        check(roster.size() >= 24, "roster too small");
        std::set<std::string> names, homes;
        std::set<int> ids;
        for (std::size_t i = 0; i < roster.size(); ++i) {
            const auto& r = roster[i];
            check(r.id == static_cast<int>(i) + 1 && ids.insert(r.id).second, "ids must be 1..N in order");
            check(names.insert(r.name).second && homes.insert(r.homeworld).second, std::string("duplicate name/homeworld ") + r.name);
            check(botf::minors::raceById(r.id) == &r && botf::minors::raceByName(r.name) == &r, "lookup");
            check(r.techLevel >= 1 && r.techLevel <= botf::MaxTechLevel && r.population > 0 && r.population <= 500 && r.bonusValue > 0,
                  std::string("numbers ") + r.name);
            check(r.affinity[0] == 0, "affinity slot 0 unused");
            for (int a : r.affinity) check(a >= -2 && a <= 2, "affinity range");
            if (r.referenceRaceId >= 0) {
                check(static_cast<std::size_t>(r.referenceRaceId) < botf::referenceData::RaceCatalogue.size() &&
                      std::string(botf::referenceData::RaceCatalogue[static_cast<std::size_t>(r.referenceRaceId)].name) == r.name,
                      std::string("RaceCatalogue reference ") + r.name);
            }
            if (r.signatureShipId >= 0) {
                check(static_cast<std::size_t>(r.signatureShipId) < botf::referenceData::ShipCatalogue.size() &&
                      botf::referenceData::ShipCatalogue[static_cast<std::size_t>(r.signatureShipId)].raceId == r.referenceRaceId,
                      std::string("signature ship belongs to the race ") + r.name);
            }
            checkOriginalProse(r.description, std::string("minor ") + r.name);
            check(!botf::minors::describeJoinBonus(r).empty() && std::string(botf::minors::temperamentName(r.temperament)) != "Unknown",
                  "display names");
        }
        // Federation-era members of BOTF's minor pool are minors here, not majors.
        check(botf::minors::raceByName("Vulcans") && botf::minors::raceByName("Andorians") && botf::minors::raceByName("Bajorans"), "classic minors");
    }, passed);

    run("placement is deterministic by seed and scaled to galaxy size", [] {
        struct Size { int w, h, systems; };
        for (const auto size : {Size{12, 8, 14}, Size{16, 12, 28}, Size{20, 15, 42}, Size{24, 16, 56}}) {
            for (std::uint64_t seed : {1ULL, 1999ULL, 424242ULL}) {
                const auto a = fresh(seed, 2, false, botf::RulesetPreset::Classic, size.w, size.h, size.systems);
                const auto b = fresh(seed, 2, false, botf::RulesetPreset::Classic, size.w, size.h, size.systems);
                check(botf::serialize(a) == botf::serialize(b), "same seed, same galaxy");
                check(static_cast<int>(a.minors.size()) == botf::minors::rosterTarget(size.systems),
                      "minor count " + std::to_string(a.minors.size()) + " for " + std::to_string(size.systems) + " systems");
                std::set<int> races;
                for (const auto& m : a.minors) {
                    const auto* r = botf::minors::raceFor(m);
                    check(botf::minors::managed(m) && r && r->id == m.raceId && races.insert(m.raceId).second, "managed, unique races");
                    check(systemOf(a, m.systemId).name == r->homeworld, "home system carries the homeworld name");
                    check(m.population == r->population && m.techLevel == r->techLevel && m.memberOf == 0 && !botf::colonyAt(a, m.systemId),
                          "fresh independent minor");
                    for (const auto& c : a.colonies)
                        check(m.name == "Bynars" || distance(systemOf(a, c.systemId), systemOf(a, m.systemId)) >= 2, "room around empire homes");
                    const int xeno = r->temperament == Temperament::Xenophobic ? 5 : 0;
                    for (int e = 1; e <= 5; ++e)
                        check(m.name == "Bynars" || m.attitude[static_cast<std::size_t>(e)] == 60 + 5 * r->affinity[static_cast<std::size_t>(e)] - xeno,
                              "initial attitude follows affinity");
                }
                for (const auto preset : {botf::RulesetPreset::Classic, botf::RulesetPreset::Remastered}) {
                    const auto ref = fresh(seed, 2, true, preset, size.w, size.h, size.systems);
                    check(ref.minors.size() == a.minors.size(), "reference campaigns and both presets place the same roster");
                    for (std::size_t i = 0; i < ref.minors.size(); ++i)
                        check(ref.minors[i].raceId == a.minors[i].raceId && ref.minors[i].systemId == a.minors[i].systemId, "same placement");
                }
                // The legacy generators keep the pre-roster baseline: only the unmanaged starter minor.
                const auto legacy = botf::generateReferenceCampaign(seed, 2, size.w, size.h, size.systems);
                check(std::none_of(legacy.minors.begin(), legacy.minors.end(), [](const botf::Minor& m) { return botf::minors::managed(m); }) &&
                      legacy.minors.size() <= 1, "generateReferenceCampaign stays on the legacy baseline");
            }
        }
        check(fresh(5, 5, true, botf::RulesetPreset::Classic, 12, 8, 14).minors.size() <
              fresh(5, 5, true, botf::RulesetPreset::Classic, 32, 24, 90).minors.size(), "bigger galaxies hold more minors");
        std::set<std::vector<int>> selections;
        for (std::uint64_t seed = 1; seed <= 12; ++seed) {
            std::vector<int> ids;
            for (const auto& m : fresh(seed).minors) ids.push_back(m.raceId);
            std::sort(ids.begin(), ids.end());
            selections.insert(ids);
        }
        check(selections.size() >= 6, "different seeds meet different minors");
        // Later eras start minors further along.
        const auto supreme = fresh(1999, 5, true, botf::RulesetPreset::Classic, 16, 12, 28, botf::StartingEra::Supreme);
        for (const auto& m : supreme.minors)
            check(m.techLevel == std::min(10, botf::minors::raceFor(m)->techLevel + 4), "era offset on minor technology");
    }, passed);

    run("join bonuses apply to the member empire and vanish on secession", [] {
        for (const auto bonus : {JoinBonus::Research, JoinBonus::Industry, JoinBonus::Food, JoinBonus::Credits, JoinBonus::Morale, JoinBonus::GroundDefense}) {
            auto s = botf::generateReferenceCampaign(2024, 3);
            s.eventsEnabled = false;
            s.aiAggressionEnabled = false;
            auto& m = s.minors.front();
            const auto* race = firstRace([&](const auto& r) {
                if (r.bonus != bonus) return false;
                for (const auto& other : s.minors) if (&other != &m && other.raceId == r.id) return false;
                return true;
            });
            becomeRace(s, m, race->id);
            const int minorSystem = m.systemId;
            const int home = s.colonies.front().ownerId == 1 ? s.colonies.front().systemId
                : std::find_if(s.colonies.begin(), s.colonies.end(), [](const auto& c) { return c.ownerId == 1; })->systemId;
            const auto before = botf::economy(s, colonyOf(s, home));
            check(botf::minors::memberBonuses(s, 1).members == 0 && botf::minors::moraleTarget(s, 1) == 70, "no bonus before joining");
            join(s, m, 1);
            botf::validate(s);
            check(colonyOf(s, minorSystem).population == race->population, "joined colony brings the minor's population");
            const auto b = botf::minors::memberBonuses(s, 1);
            const auto after = botf::economy(s, colonyOf(s, home));
            check(b.members == 1, "one member");
            switch (bonus) {
                case JoinBonus::Research: check(b.researchPct == race->bonusValue && after.research > before.research, "research bonus"); break;
                case JoinBonus::Industry: check(b.industryPct == race->bonusValue && after.industry > before.industry, "industry bonus"); break;
                case JoinBonus::Food: check(b.foodPct == race->bonusValue && after.food > before.food, "food bonus"); break;
                case JoinBonus::Credits: check(b.creditsPct == race->bonusValue && after.income > before.income, "credit bonus"); break;
                case JoinBonus::Morale: check(botf::minors::moraleTarget(s, 1) == 70 + race->bonusValue, "morale bonus"); break;
                case JoinBonus::GroundDefense:
                    check(botf::minors::invasionDefense(s, colonyOf(s, home), 100) == 100 + race->bonusValue, "ground defense bonus"); break;
            }
            check(botf::minors::memberBonuses(s, 2).members == 0, "other empires gain nothing");
            // Neglect: loyalty collapses and the council secedes at the next review (turn 5).
            s.minors.front().attitude[1] = 0;
            s.turn = 4; s.diplomacyEngineState.currentTurn = 4;
            botf::syncDiplomacyState(s);
            botf::validate(s);
            botf::advance(s);
            check(s.minors.front().memberOf == 0 && !botf::colonyAt(s, minorSystem), "member seceded");
            const auto gone = botf::minors::memberBonuses(s, 1);
            check(gone.members == 0 && gone.researchPct == 0 && gone.industryPct == 0 && gone.foodPct == 0 && gone.creditsPct == 0 &&
                  gone.morale == 0 && gone.groundDefensePct == 0 && botf::minors::moraleTarget(s, 1) == 70, "bonus reversed on secession");
            check(botf::minors::invasionDefense(s, colonyOf(s, home), 100) == 100, "defense bonus reversed");
        }
    }, passed);

    run("membership through the real pipeline uses the roster profile", [] {
        auto s = fresh(55);
        auto& m = s.minors.front();
        const int minorSystem = m.systemId;
        m.contact[1] = true; m.attitude[1] = 95; m.friendshipTurn[1] = 1;
        s.turn = 2;
        botf::syncDiplomacyState(s);
        botf::submit(s, botf::makeProposeTreatyCommand(1, minorSystem, true, botf::diplomacy::TreatyType::Membership));
        botf::advance(s);
        check(s.minors.front().memberOf == 1, "joined");
        check(botf::colonyAt(s, minorSystem)->population == std::max(10, s.minors.front().population), "roster population");
        check(botf::minors::memberBonuses(s, 1).members == 1, "bonus active");
    }, passed);

    run("legacy minors keep legacy rules and saves carry no roster section", [] {
        auto s = botf::generate(1999);
        botf::Minor m; m.systemId = 2; m.name = "Bynars"; m.contact[1] = true; s.minors.push_back(m);
        botf::validate(s);
        check(botf::serialize(s).find("MINOR_RACES") == std::string::npos, "no section for legacy minors");
        check(botf::minors::raceFor(s.minors.front()) == botf::minors::raceByName("Bynars") && !botf::minors::managed(s.minors.front()),
              "legacy minors resolve by name for display");
        s.turn = 5;
        const auto before = s.minors.front();
        std::vector<std::string> events;
        botf::minors::reviewMinorRaces(s, events);
        check(s.minors.front().attitude == before.attitude && s.minors.front().techLevel == 0 && events.empty(), "legacy minors do not progress");
        botf::Colony c; botf::minors::configureJoinedColony(s.minors.front(), c);
        check(c.population == botf::Colony{}.population, "legacy join keeps defaults");
        s.minors.front().memberOf = 1;
        check(botf::minors::memberBonuses(s, 1).researchPct == 10, "legacy +10% research dividend preserved");
        botf::Minor unknown; unknown.name = "Somebody"; unknown.systemId = 3; unknown.memberOf = 1; s.minors.push_back(unknown);
        check(botf::minors::memberBonuses(s, 1).researchPct == 20, "unknown legacy minors keep the generic dividend");
    }, passed);

    run("old saves keep their minors exactly and load byte-identically", [] {
        for (const bool reference : {false, true}) {
            auto s = reference ? botf::generateReferenceCampaign(8080, 3) : botf::generateCampaign(8080, 3);
            for (int t = 0; t < 11; ++t) botf::advance(s);
            const auto text = botf::serialize(s);
            check(text.find("MINOR_RACES") == std::string::npos, "legacy campaigns write no roster section");
            const auto loaded = botf::deserialize(text);
            check(botf::serialize(loaded) == text, "byte-identical reload");
            check(loaded.minors.size() == s.minors.size(), "same minors");
            for (std::size_t i = 0; i < s.minors.size(); ++i) {
                const auto& a = s.minors[i];
                const auto& b = loaded.minors[i];
                check(a.name == b.name && a.systemId == b.systemId && a.memberOf == b.memberOf && a.attitude == b.attitude &&
                      !botf::minors::managed(b) && b.techLevel == 0 && b.population == 0, "legacy minor unchanged");
            }
        }
    }, passed);

    run("minors grow and advance over time, deterministically across save/load", [] {
        auto s = fresh(31337, 2, true, botf::RulesetPreset::Remastered);
        s.aiAggressionEnabled = false;
        const auto start = s.minors;
        auto resumed = botf::deserialize(botf::serialize(s));
        for (int t = 0; t < 60; ++t) { // Twelve council reviews
            botf::advance(s); botf::advance(resumed);
            if (t == 17) resumed = botf::deserialize(botf::serialize(resumed));
            check(botf::serialize(s) == botf::serialize(resumed), "diverged at turn " + std::to_string(s.turn));
        }
        for (std::size_t i = 0; i < s.minors.size(); ++i) {
            const auto& now = s.minors[i];
            const auto& then = start[i];
            check(now.raceId == then.raceId, "identity stable");
            if (now.memberOf == 0) check(now.population > then.population, std::string("population grows: ") + now.name);
            check(now.techLevel > then.techLevel || then.techLevel == botf::MaxTechLevel, std::string("technology advances: ") + now.name);
        }
    }, passed);

    run("scientific minors out-research others and level-ups are reported", [] {
        auto s = botf::generateReferenceCampaign(7);
        auto& m = s.minors.front();
        const auto* sci = firstRace([&](const auto& r) {
            if (r.temperament != Temperament::Scientific || r.techLevel > 7) return false;
            for (const auto& o : s.minors) if (&o != &m && o.raceId == r.id) return false;
            return true;
        });
        becomeRace(s, m, sci->id);
        m.contact[static_cast<std::size_t>(s.playerEmpire)] = true;
        const int level = m.techLevel;
        std::vector<std::string> events;
        for (int turn = 5; turn <= 5 * (level + 2); turn += 5) { s.turn = turn; botf::minors::reviewMinorRaces(s, events); }
        check(m.techLevel == level + 1, "scientific pace: two progress per review");
        check(!events.empty() && events.back().find("technology level") != std::string::npos, "level-up reported");
        s.turn = 7; const auto snapshot = s.minors; botf::minors::reviewMinorRaces(s, events);
        check(s.minors.front().progress == snapshot.front().progress, "reviews only every five turns");
    }, passed);

    run("affinity and xenophobia pull independent attitudes; treaties shield against aversion", [] {
        auto s = botf::generateCampaign(99);
        auto& m = s.minors.front();
        if (m.raceId != botf::minors::raceByName("Vulcans")->id) {
            for (auto& o : s.minors) if (o.raceId == botf::minors::raceByName("Vulcans")->id) becomeRace(s, o, firstRace([&](const auto& r) {
                for (const auto& x : s.minors) if (x.raceId == r.id) return false;
                return true;
            })->id);
            becomeRace(s, m, botf::minors::raceByName("Vulcans")->id);
        }
        m.contact[1] = m.contact[2] = true;
        m.attitude[1] = 50; m.attitude[2] = 50;
        s.turn = 5;
        std::vector<std::string> events;
        botf::minors::reviewMinorRaces(s, events);
        check(m.attitude[1] == 52, "Vulcans warm to the Federation (+2 toward 70)");
        check(m.attitude[2] == 49, "and cool toward the Klingons (-1 toward 40)");
        m.attitude[1] = 70; m.friendshipTurn[2] = 0; m.attitude[2] = 55;
        s.turn = 10; botf::minors::reviewMinorRaces(s, events);
        check(m.attitude[1] == 70 && m.attitude[2] == 55, "goal reached; a treaty shields against aversion");
    }, passed);

    run("warlike members guard their homeworld against invasion", [] {
        auto s = botf::generateCampaign(4242);
        auto& m = s.minors.front();
        const auto* warlike = firstRace([&](const auto& r) {
            if (r.temperament != Temperament::Warlike) return false;
            for (const auto& o : s.minors) if (&o != &m && o.raceId == r.id) return false;
            return true;
        });
        becomeRace(s, m, warlike->id);
        join(s, m, 1);
        botf::validate(s);
        const int pct = botf::minors::memberBonuses(s, 1).groundDefensePct;
        check(botf::minors::invasionDefense(s, colonyOf(s, m.systemId), 40) == (40 + 5 + 2 * m.techLevel) * (100 + pct) / 100, "home guard");
        const auto* peaceful = firstRace([&](const auto& r) {
            if (r.temperament != Temperament::Peaceful || r.bonus == JoinBonus::GroundDefense) return false;
            for (const auto& o : s.minors) if (&o != &m && o.raceId == r.id) return false;
            return true;
        });
        becomeRace(s, m, peaceful->id);
        check(botf::minors::invasionDefense(s, colonyOf(s, m.systemId), 40) == 40, "no guard for peaceful members");
    }, passed);

    run("MINOR_RACES save section: round trip, optional on load, strict validation", [] {
        auto s = fresh(1999, 5, true, botf::RulesetPreset::Remastered);
        for (int t = 0; t < 12; ++t) botf::advance(s);
        const auto text = botf::serialize(s);
        check(text.find("\nMINOR_RACES " + std::to_string(s.minors.size()) + "\n") != std::string::npos, "section written");
        const auto loaded = botf::deserialize(text);
        check(botf::serialize(loaded) == text, "round trip");
        for (std::size_t i = 0; i < s.minors.size(); ++i) {
            const auto& a = s.minors[i];
            const auto& b = loaded.minors[i];
            check(a.raceId == b.raceId && a.techLevel == b.techLevel && a.population == b.population && a.progress == b.progress, "fields");
        }
        // A save written before the roster existed simply lacks the section: its minors load as legacy minors.
        const auto start = text.find("MINOR_RACES ");
        const auto end = text.find("CHECKSUM ");
        auto older = text.substr(0, start) + text.substr(end);
        const auto legacy = botf::deserialize(withChecksum(older));
        for (const auto& m : legacy.minors) check(!botf::minors::managed(m) && m.techLevel == 0, "legacy minors");
        // Corruptions are rejected.
        auto corrupt = [&](const std::function<std::string(std::string)>& edit) {
            rejects([&] { botf::deserialize(withChecksum(edit(text))); }, "corrupt MINOR_RACES accepted");
        };
        const auto row = text.find('\n', start) + 1;
        const auto rowEnd = text.find('\n', row);
        std::string firstRow = text.substr(row, rowEnd - row);
        corrupt([&](std::string t) { return t.replace(row, rowEnd - row, firstRow.substr(0, firstRow.find(' ')) + " 99 1 10 0"); });
        corrupt([&](std::string t) { return t.replace(row, rowEnd - row, firstRow.substr(0, firstRow.find(' ')) + " " +
                                           std::to_string(s.minors.front().raceId) + " 11 10 0"); });
        corrupt([&](std::string t) { return t.replace(start, row - start, "MINOR_RACES 0\n"); });
        const auto row2End = text.find('\n', rowEnd + 1);
        corrupt([&](std::string t) { return t.replace(rowEnd + 1, row2End - rowEnd - 1, firstRow); }); // Duplicate row
        corrupt([&](std::string t) { return t.replace(row, rowEnd - row, "9999 1 1 10 0"); });
    }, passed);

    if (failed) {
        std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl;
        return 1;
    }
    std::cout << "ALL " << passed << " CONTENT SCENARIOS PASSED" << std::endl;
    return 0;
}
