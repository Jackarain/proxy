/*
    Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/boostorg/site-docs
*/

const path = require('path');
const fs = require('fs');
const xpath = require('xpath');
const DOMParser = require('xmldom').DOMParser;

/*
    Load tag files

    Default tag files come from the cpp_tags directory.
    We still need to implement other strategies and sources for tag files.

    To include cppreference tags you can get them from
    https://en.cppreference.com/w/Cppreference:Archives or generate a
    more recent version using the following commands:

    ```
    git clone https://github.com/PeterFeicht/cppreference-doc
    cd cppreference-doc
    make source
    make doc_doxygen
    ```

    The result will be in ./output.
 */
let tagDocs = {}
// Read all files in __dirname
const defaultTagsDir = path.join(__dirname, 'cpp_tags');
fs.readdirSync(defaultTagsDir).forEach(file => {
    if (file.endsWith('.tag.xml')) {
        const tagFile = path.join(defaultTagsDir, file);
        const xml = fs.readFileSync(tagFile, 'utf8');
        tagDocs[file.replace('.tag.xml', '')] = new DOMParser().parseFromString(xml, 'text/xml');
    }
})

/**
 * Gets the URL for a symbol.
 *
 * @param doc - The XML document containing the symbols.
 * @param symbolName - The name of the symbol.
 * @returns {undefined|string} The URL for the symbol, or undefined if the symbol is not found.
 */
function getSymbolLink(doc, symbolName) {
    const select = xpath.useNamespaces({'ns': 'http://www.w3.org/1999/xhtml'})
    let result = select(`//compound[name="${symbolName}"]`, doc)
    if (result.length > 0) {
        const symbolNode = result[0]
        const symbolName = symbolNode.getElementsByTagName('name')[0].textContent
        const symbolFilename = symbolNode.getElementsByTagName('filename')[0].textContent
        return `https://en.cppreference.com/w/${symbolFilename}[${symbolName},window="_blank"]`
    }
    return undefined
}

/**
 * Registers an inline macro named "cpp" with the given Asciidoctor registry.
 * This macro creates a link to the corresponding Boost C++ symbol using the
 * provided Doxygen tag files.
 *
 * @param {Object} registry - The Asciidoctor registry to register the macro with.
 * @throws {Error} If registry is not defined.
 * @example
 * const asciidoctor = require('asciidoctor');
 * const registry = asciidoctor.Extensions.create();
 * registerBoostMacro(registry);
 */
module.exports = function (registry) {
    // Make sure registry is defined
    if (!registry) {
        throw new Error('registry must be defined');
    }

    /**
     * Processes the "cpp" inline macro.
     * If the "title" attribute is specified, it is used as the link text.
     * Otherwise, the monospace code target is used as the link text.
     * The link URL is constructed based on the snake_case version of the target.
     *
     * @param {Object} parent - The parent node of the inline macro.
     * @param {string} target - The target of the inline macro.
     * @param {Object} attr - The attributes of the inline macro.
     * @returns {Object} An inline node representing the link.
     */
    registry.inlineMacro('cpp', function () {
        const self = this;
        self.process(function (parent, target, attr) {
            // const DEFAULT_BOOST_BRANCH = 'master'
            // const branch = parent.getDocument().getAttribute('page-boost-branch', DEFAULT_BOOST_BRANCH)
            for (const [tag, doc] of Object.entries(tagDocs)) {
                const link = getSymbolLink(doc, target)
                if (link) {
                    return self.createInline(parent, 'quoted', link, {type: 'monospaced'})
                }
            }
            if (['bool', 'char', 'char8_t', 'char16_t', 'char32_t', 'wchar_t', 'int', 'signed', 'unsigned', 'short', 'long', 'float', 'true', 'false', 'double', 'void'].includes(target)) {
                return self.createInline(parent, 'quoted', `https://en.cppreference.com/w/cpp/language/types[${target},window=_blank]`, {type: 'monospaced'})
            }
            return self.createInline(parent, 'quoted', target, {type: 'monospaced'})
        })
    })
}

