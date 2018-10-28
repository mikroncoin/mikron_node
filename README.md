<hr />
<div align="center">
    <img src="images/mikron_logo_white.svg" alt="Logo" width='250px' height='auto' bgcolor="black"/>
</div>
<hr />

### What is Mikron?

**Mikron** is a decentralized block-lattice digital currency with fast and cheap transactions,
focusing on community building.  See the Mikron website at https://mikron.io .

Mikron is based on the great **Nano** (by Colin LeMahieu; formerly known as RaiBlocks; http://nano.org).

### TODO Update Readme

[![Build Status](https://travis-ci.org/nanocurrency/raiblocks.svg?branch=master)](https://travis-ci.org/nanocurrency/raiblocks)
[![Build status](https://ci.appveyor.com/api/projects/status/uwdcxi87h2y6jx94/branch/master?svg=true)](https://ci.appveyor.com/project/argakiig/raiblocks/branch/master)

### Key Features

* Mikron utilizes a [block-lattice](https://github.com/mikroncoin/mikron_node/wiki/Block-lattice) architecture, unlike conventional blockchains used in many other cryptocurrencies.
* The network requires minimal resources, no high-power mining hardware, and can process high transaction throughput.
* Offers instantaneous transactions with zero fees and unlimited scalability, making Mikron an ideal solution for peer-to-peer transactions.

### Key Features -- Nano

* As of May 2018, the Nano network has processed over ten million transactions with an unpruned ledger size of only 3.8GB.

For more information, see [Nano.org](https://nano.org/) or read the [whitepaper](https://nano.org/en/whitepaper).

### Guides & Documentation

* White Paper
* [Build Instructions](https://github.com/mikroncoin/mikron_node/wiki/Build-Instructions)
* Command Line Interface
* RPC Protocol
* Wallet Design
* Block Lattice
* Design Features

### Guides & Documentation -- Nano

* [White Paper](https://nano.org/en/whitepaper)
* [Build Instructions](https://github.com/nanocurrency/raiblocks/wiki/Build-Instructions)
* [Command Line Interface](https://github.com/nanocurrency/raiblocks/wiki/Command-line-interface)
* [RPC Protocol](https://github.com/nanocurrency/raiblocks/wiki/RPC-protocol)
* [Wallet Design](https://github.com/nanocurrency/raiblocks/wiki/Wallet-design)
* [Block Lattice](https://github.com/nanocurrency/raiblocks/wiki/Block-lattice)
* [Design Features](https://github.com/nanocurrency/raiblocks/wiki/Design-features)

### Links & Resources

* [Mikron Website](https://mikron.io)
* [Discord Chat](https://discord.gg/QBKr3hv)
* [GitHub wiki](https://github.com/mikroncoin/mikron_node/wiki)
* [Nano GitHub](https://github.com/nanocurrency/raiblocks)
* (Roadmap)
* (Reddit)
* (Medium)
* (Twitter)
* (Forum)

### FAQ

#### How is Mikron different from Nano?
Mikron has different targeted objectives -- community building, reward point handling; and some important differences, such as continuous reward genesis ('manna'), different balance representation, block creation times, etc.  See the [differences Wiki page](https://github.com/mikroncoin/mikron_node/wiki/Differences-to-Nano) for more details.

#### Why is the mikron_node source repo not a 'GitHub fork' of the original Nano repo?
Git is a great source control system, very flexible with regards to repository migration (with full history), and supporting transfering branches between different origin repositories.  The GitHub fork is an even handier feature for forking repositories.  However, the mikron_node repository is not a 'GitHub fork' of the original Nano.  The rationale for this is that being a hard fork with some fundamental changes, downstream merging is not planned as a regular feature, visibility of the upstream master branch is not desired.  The Nano repository is nonetheless easy to find from Mikron pages, and easy to work with.  For example, consider the commands:

    git checkout upstream
    git pull https://github.com/nanocurrency/raiblocks.git master

#### What is the relationship to MicroCoin?
Mikron and [MicroCoin](https://mikrocoin.hu) are different, independent projects, based on completely different technology.  Both are based in Hungary, and there are some persons who contributed to both, but they are separate projects.  Despite the name similarity, they should not be confused with each other.

### Links & Resources -- Nano

* [Nano Website](https://nano.org)
* [Nano Roadmap](https://developers.nano.org/roadmap)
* [Discord Chat](https://chat.nano.org/)
* [Reddit](https://reddit.com/r/nanocurrency)
* [Medium](https://medium.com/nanocurrency)
* [Twitter](https://twitter.com/nano)
* [Forum](https://forum.raiblocks.net/)
* [GitHub wiki](https://github.com/nanocurrency/raiblocks/wiki)

### Want to Contribute? -- Nano

Please see the [contributors guide](https://github.com/nanocurrency/raiblocks/wiki/Contributing).

### Contact us -- Nano

We want to hear about any trouble, success, delight, or pain you experience when
using Nano. Let us know by [filing an issue](https://github.com/nanocurrency/raiblocks/issues), joining us in [reddit](https://reddit.com/r/nanocurrency), or joining us in [Discord](https://chat.nano.org/).
