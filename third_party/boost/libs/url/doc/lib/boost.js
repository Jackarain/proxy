/*
    Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
    Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/boostorg/antora
*/

'use strict'

const fs = require('node:fs');
const path = require('node:path');
const process = require("process");
const {execSync} = require('child_process');

/*
    This extension allows every asciidoc attribute
    specified in the playbook or the command line
    to become a variable in the playbook using the
    syntax ${var}.
*/
class BoostPlaybookPatterns {
    static register() {
        new BoostPlaybookPatterns(this)
    }

    constructor(ctx) {
        ;(this.ctx = ctx)
            .on('contextStarted', this.contextStarted.bind(this))
    }

    /**
     * Recursively replaces all occurrences of a pattern with a substitution string
     * in an object or array of strings.
     *
     * @param {Array|Object} obj - The object or array to modify.
     * @param {string|RegExp} pattern - The pattern to search for.
     * @param {string} replacement - The replacement string.
     * @returns {void}
     */
    replaceAll(obj, pattern, replacement) {
        if (Array.isArray(obj)) {
            obj.forEach((val, i) => {
                if (typeof val === "string") obj[i] = val.replaceAll(pattern, replacement);
                else if (typeof val === "object") this.replaceAll(val, pattern, replacement);
            });
        } else if (typeof obj === "object" && obj !== null) {
            Object.entries(obj).forEach(([key, val]) => {
                if (typeof val === "string") obj[key] = val.replaceAll(pattern, replacement);
                else if (typeof val === "object") this.replaceAll(val, pattern, replacement);
            });
        }
    }

    /**
     * Sets page attributes on the playbook's `asciidoc` object based on the current Git branch and commit.
     * Also sets the `branches` and `tags` properties of the playbook's `content` object, and optionally
     * modifies the playbook's `urls` object.
     *
     * @param {object} playbook - The playbook object to modify.
     */
    setPageAttributes(playbook) {
        let branch = playbook.asciidoc.attributes['page-boost-branch'];
        if (!branch)
            try {
                branch = execSync('git rev-parse --abbrev-ref HEAD').toString().trim();
            } catch (error) {
                branch = 'develop'
            }
        playbook.asciidoc.attributes['page-boost-branch'] = branch
        playbook.content.branches = branch

        let commit_id = playbook.asciidoc.attributes['page-commit-id'];
        if (!commit_id)
            try {
                commit_id = execSync('git rev-parse HEAD').toString().trim().substring(0, 7);
                playbook.asciidoc.attributes['page-commit-id'] = commit_id
            } catch (error) {
                playbook.asciidoc.attributes['page-commit-id'] = undefined
            }

        if (branch in ["master", "develop"]) {
            playbook.content.tags = ""
        } else {
            playbook.content.tags = "boost-" + branch
            playbook.urls.latestVersionSegment = "" // disable tag in URL
        }
    }

    /**
     * Sets the `url` property of each source in the playbook's content object
     * to `'.'` if an `antora.yml` file exists in the source's directory.
     *
     * @param {object} playbook - The playbook object to modify.
     */
    setContentSourceURLs(playbook) {
        const cwd = process.cwd();
        const sources = playbook.content.sources;
        for (let i = 0; i < sources.length; i++) {
            const source = sources[i];
            const antoraYmlPath = path.join(cwd, source.startPath, 'antora.yml');
            if (fs.existsSync(antoraYmlPath)) {
                source.url = '.';
            }
        }
    }

    /**
     * Recursively replaces all occurrences of attribute placeholders in a playbook
     * with their corresponding attribute values.
     *
     * @param {Object} playbook - The playbook to modify.
     * @returns {void}
     */
    replacePatterns(playbook) {
        for (const [key, value] of Object.entries(playbook.asciidoc.attributes)) {
            const placeholder = "${" + key + "}";
            this.replaceAll(playbook.site, placeholder, value);
            this.replaceAll(playbook.urls, placeholder, value);
            this.replaceAll(playbook.content, placeholder, value);
            this.replaceAll(playbook.output, placeholder, value);
            this.replaceAll(playbook.ui, placeholder, value);
        }
    }

    /**
     * Called when Antora starts processing a playbook. Sets page attributes, content source URLs,
     * and performs replacements in the playbook's content.
     *
     * @param {object} context - An object containing the playbook and other contextual information.
     * @param {object} context.playbook - The playbook object being processed.
     */
    contextStarted({playbook}) {
        this.setPageAttributes(playbook)
        this.setContentSourceURLs(playbook);
        this.replacePatterns(playbook);
    }
}

module.exports = BoostPlaybookPatterns
