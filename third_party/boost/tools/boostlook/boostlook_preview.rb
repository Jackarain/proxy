#!/usr/bin/env ruby

require 'asciidoctor'
require 'listen'
require 'pathname'
require 'logger'

# AsciidocRenderer handles building AsciiDoc files, monitoring changes, and rendering the output in a browser.
class AsciidocRenderer
  # Define our relevant constant paths
  PATHS = {
    jamfile: 'doc/Jamfile',
    specimen_docinfo_footer: 'doc/specimen-docinfo-footer.html',
    specimen_adoc: 'doc/specimen.adoc',
    css: 'boostlook.css',
    boostlook_rb: 'boostlook.rb',
    output_dir: 'doc/html'
  }.freeze

  # OS-specific commands to open the default web browser
  OS_BROWSER_COMMANDS = {
    /darwin/       => 'open',       # macOS
    /linux/        => 'xdg-open',   # Linux
    /mingw|mswin/  => 'start'        # Windows
  }.freeze

  def initialize
    # Initialize the logger
    @logger = Logger.new($stdout)
    @logger.level = Logger::INFO
    @logger.formatter = ->(_, _, _, msg) { "#{msg}\n" }

    @file_opened = false          # Flag to prevent multiple browser openings
    @shutdown_requested = false   # Flag to handle graceful shutdown
    validate_b2                    # Ensure Boost.Build is installed
  end

  # Entry point to run the renderer
  def run
    validate_files      # Check for the presence of required files
    initial_build       # Perform the initial build and render
    setup_signal_traps  # Setup signal handlers for graceful shutdown
    watch_files         # Start watching for file changes
  end

  private

  # Validates that all required files are present
  def validate_files
    required_files = [PATHS[:jamfile], PATHS[:specimen_docinfo_footer], PATHS[:specimen_adoc], PATHS[:css], PATHS[:boostlook_rb]]
    missing_files = required_files.reject { |file| File.exist?(file) }
    unless missing_files.empty?
      missing_files.each { |file| @logger.error("Required file #{file} not found") }
      exit 1
    end
  end

  # Checks if the 'b2' command (Boost.Build) is available
  def validate_b2
    unless system('which b2 > /dev/null 2>&1')
      @logger.error("'b2' command not found. Please install Boost.Build and ensure it's in your PATH.")
      exit 1
    end
  end

  # Builds the AsciiDoc project using Boost.Build
  def build_asciidoc
    Dir.chdir('doc') do
      if system('b2')
        @logger.info("Build successful")
        true
      else
        @logger.error("Build failed")
        false
      end
    end
  end

  # Opens the generated HTML file in the default web browser
  def open_in_browser
    return if @file_opened

    cmd = OS_BROWSER_COMMANDS.find { |platform, _| RUBY_PLATFORM =~ platform }&.last
    if cmd
      system("#{cmd} #{PATHS[:output_dir]}/specimen.html")
      @file_opened = true
    else
      @logger.warn("Unsupported OS. Please open #{PATHS[:output_dir]}/specimen.html manually")
    end
  end

  # Performs the initial build and opens the result in the browser
  def initial_build
    if build_asciidoc && File.exist?("#{PATHS[:output_dir]}/specimen.html")
      open_in_browser
      @logger.info("Rendered #{PATHS[:specimen_adoc]} to #{PATHS[:output_dir]}/specimen.html")
    else
      @logger.error("Failed to generate #{PATHS[:output_dir]}/specimen.html")
    end
  end

  # Sets up file listeners to watch for changes and trigger rebuilds
  def watch_files
    @listener = Listen.to('doc', '.') do |modified, added, removed|
      handle_file_changes(modified, added, removed)
    end

    @listener.ignore(/doc\/html/) # Ignore changes in the output directory
    @listener.start
    @logger.info("Watching for changes in 'doc' and root directories (excluding 'doc/html')...")

    # Keep the script running until a shutdown is requested
    until @shutdown_requested
      sleep 1
    end

    shutdown # Perform shutdown procedures
  end

  # Handles detected file changes by determining if a rebuild is necessary
  def handle_file_changes(modified, added, removed)
    @logger.debug("Modified files: #{modified.join(', ')}")
    @logger.debug("Added files: #{added.join(', ')}") if added.any?
    @logger.debug("Removed files: #{removed.join(', ')}") if removed.any?

    relevant_files = [
      File.expand_path(PATHS[:jamfile]),
      File.expand_path(PATHS[:specimen_docinfo_footer]),
      File.expand_path(PATHS[:specimen_adoc]),
      File.expand_path(PATHS[:css]),
      File.expand_path(PATHS[:boostlook_rb])
    ]

    # Check if any of the changed files are relevant for rebuilding
    changes_relevant = (modified + added + removed).any? do |file|
      relevant_files.include?(File.expand_path(file))
    end

    if changes_relevant
      @logger.info("Relevant changes detected, rebuilding...")
      if build_asciidoc && File.exist?("#{PATHS[:output_dir]}/specimen.html")
        open_in_browser
        @logger.info("Re-rendered successfully")
      end
    end
  end

  # Sets up signal traps to handle graceful shutdown on interrupt or terminate signals
  def setup_signal_traps
    Signal.trap("INT") { @shutdown_requested = true }
    Signal.trap("TERM") { @shutdown_requested = true }
  end

  # Performs shutdown procedures, such as stopping the file listener
  def shutdown
    @logger.info("Shutting down...")
    @listener.stop if @listener
    exit
  end
end

# Instantiate and run the AsciidocRenderer
AsciidocRenderer.new.run
