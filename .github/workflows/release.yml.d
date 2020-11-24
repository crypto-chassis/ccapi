name: Release
on:
  push:
    branches:
      - master
jobs:
  release:
    name: release
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v1
      - uses: actions/setup-node@v1
      - run: 'echo ''{
          "name": "ccapi_cpp",
          "version": "1.0.0",
          "devDependencies": {
            "semantic-release": "^17.1.1",
            "conventional-changelog-conventionalcommits":"^4.4.0"
          },
          "release": {
            "branches": ["master"],
            "plugins": [["@semantic-release/commit-analyzer", {
                        "preset": "conventionalcommits",
                        "releaseRules": [
                          {"type": "fix", "release": "patch"},
                          {"type": "feat", "release": "minor"},
                          {"type": "build", "release": "patch"},
                          {"type": "chore", "release": "patch"},
                          {"type": "ci", "release": "patch"},
                          {"type": "docs", "release": "patch"},
                          {"type": "style", "release": "patch"},
                          {"type": "refactor", "release": "patch"},
                          {"type": "perf", "release": "patch"},
                          {"type": "test", "release": "patch"}
                        ]
                      }], "@semantic-release/github"]
          }
        }'' > "package.json"'
      - run: npm install
      - run: npm ci
      - env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: npx semantic-release
