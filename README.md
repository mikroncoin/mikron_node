<hr />
<div align="center">
    <img src="images/mikron_logo_white.svg" alt="Logo" width='250px' height='auto' bgcolor="black"/>
</div>

<hr />

[![Build Status](https://travis-ci.org/mikroncoin/mikron_node.svg?branch=master)](https://travis-ci.org/mikroncoin/mikron_node)
[![Build status](https://ci.appveyor.com/api/projects/status/i7rk7dh6lvjxpwb8?svg=true)](https://ci.appveyor.com/project/catenocrypt/mikron-node)
[![pipeline status](https://gitlab.com/mikroncoin/mikron_node/badges/master/pipeline.svg)](https://gitlab.com/mikroncoin/mikron_node/pipelines)

### What is Mikron?

**Mikron** is a decentralized block-lattice digital currency with fast and cheap transactions,
focusing on community building.  See the Mikron website at https://mikron.io .

Mikron is based on the great **Nano** (by Colin LeMahieu; formerly known as RaiBlocks; http://nano.org).

### TODO Update Readme

### Key Features

* Mikron utilizes a [block-lattice](https://github.com/mikroncoin/mikron_node/wiki/Block-lattice) architecture, unlike conventional blockchains used in many other cryptocurrencies.
* The network requires minimal resources, no high-power mining hardware, and can process high transaction throughput.
* Offers instantaneous transactions with zero fees and unlimited scalability, making Mikron an ideal solution for peer-to-peer transactions.

### Key Features -- Nano

* As of May 2018, the Nano network has processed over ten million transactions with an unpruned ledger size of only 3.8GB.

For more information, see [Nano.org](https://nano.org/) or read the [whitepaper](https://nano.org/en/whitepaper).

### Guides & Documentation

* [White Paper](https://mikron.io/download/Mikron_Technical_Paper_ENG.pdf)
* [Build Instructions](https://github.com/mikroncoin/mikron_node/wiki/Build-Instructions)
* Command Line Interface
* RPC Protocol
* Wallet Design
* [Block Lattice](https://github.com/mikroncoin/mikron_node/wiki/Block-Lattice)
* Design Features

### Guides & Documentation -- Nano

* [White Paper](https://nano.org/en/whitepaper)
* [Command Line Interface](https://github.com/nanocurrency/raiblocks/wiki/Command-line-interface)
* [RPC Protocol](https://github.com/nanocurrency/raiblocks/wiki/RPC-protocol)
* [Wallet Design](https://github.com/nanocurrency/raiblocks/wiki/Wallet-design)
* [Design Features](https://github.com/nanocurrency/raiblocks/wiki/Design-features)

### Links & Resources

* [Mikron Website](https://mikron.io)
* [Discord Chat](https://discord.gg/QBKr3hv)
* [GitHub](https://github.com/mikroncoin/mikron_node)
* [GitHub wiki](https://github.com/mikroncoin/mikron_node/wiki)
* [Nano GitHub](https://github.com/nanocurrency/raiblocks)
* [Roadmap](https://mikron.io/#reszletes-program)
* (Reddit)
* [Medium](https://medium.com/@mikronsocial)
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
Apart the similar name, not much!  Mikron and [MicroCoin](https://mikrocoin.hu) are different, independent projects, based on different technology.  Both are based in Hungary, and there are some persons who contributed to both, but they are separate projects.  So please be careful not to confuse them with each other.

### Links & Resources -- Nano

* [Nano Website](https://nano.org)
* [Reddit](https://reddit.com/r/nanocurrency)
* [GitHub wiki](https://github.com/nanocurrency/raiblocks/wiki)
* [BitcoinTalk announcement](https://bitcointalk.org/index.php?topic=1381323.0)

### Want to Contribute? -- Nano

Please see the [contributors guide](https://github.com/nanocurrency/raiblocks/wiki/Contributing).

### Contact us -- Nano

We want to hear about any trouble, success, delight, or pain you experience when
using Nano. Let us know by [filing an issue](https://github.com/nanocurrency/raiblocks/issues), joining us in [reddit](https://reddit.com/r/nanocurrency), or joining us in [Discord](https://chat.nano.org/).
