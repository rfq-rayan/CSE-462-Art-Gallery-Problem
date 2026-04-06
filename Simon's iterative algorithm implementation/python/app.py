#!/usr/bin/env python3
"""
Art Gallery Problem - Web Visualization Frontend

A Flask-based web application for visualizing polygons and guard placements.
"""

import argparse
import json
import os
import sys
import subprocess
import tempfile
from flask import Flask, request, jsonify, render_template, send_file
import matplotlib
matplotlib.use('Agg')  # Use non-interactive backend
import matplotlib.pyplot as plt
import numpy as np
from shapely.geometry import Polygon, Point
from shapely.ops import unary_union
import io
import base64

app = Flask(__name__)

# Store loaded polygons
polygons = []


def visualize_polygon(polygon_data, guards=None, output_path=None):
    """
    Visualize a polygon with optional guard positions.

    Args:
        polygon_data: dict with 'vertices' key containing list of [x, y] coordinates
        guards: Optional list of guard positions - can be [[x, y], ...] or [{'x': x, 'y': y}, ...]
        output_path: Optional path to save the visualization

    Returns:
        If output_path is None, returns base64 encoded image string
    """
    # Extract vertices
    vertices = [(v['x'], v['y']) for v in polygon_data['vertices']]

    # Normalize guards to tuple format [(x, y), ...]
    normalized_guards = None
    if guards:
        normalized_guards = []
        for g in guards:
            if isinstance(g, dict):
                normalized_guards.append((g['x'], g['y']))
            elif isinstance(g, (list, tuple)) and len(g) >= 2:
                normalized_guards.append((g[0], g[1]))

    # Create polygon
    poly = Polygon(vertices)

    # Create figure
    fig, ax = plt.subplots(1, 1, figsize=(10, 10))

    # Plot polygon
    x, y = poly.exterior.xy
    ax.fill(x, y, color='lightblue', alpha=0.5)
    ax.plot(x, y, 'b-', linewidth=2)

    # Plot vertices
    vx, vy = zip(*vertices)
    ax.scatter(vx, vy, c='blue', s=50, zorder=4)

    # Plot guards if provided
    if normalized_guards:
        gx, gy = zip(*normalized_guards)
        ax.scatter(gx, gy, c='red', s=100, zorder=5, marker='*', label='Guards')

        # Draw visibility regions for each guard (simplified)
        for guard in normalized_guards:
            guard_point = Point(guard)
            # Draw line of sight indicators
            for vertex in vertices:
                vertex_point = Point(vertex)
                # Check if line is inside polygon (simplified check)
                if poly.contains(guard_point) or poly.boundary.contains(guard_point):
                    ax.plot([guard[0], vertex[0]], [guard[1], vertex[1]],
                            'r--', alpha=0.2, linewidth=0.5)

        ax.legend()

    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_title(f'{polygon_data.get("name", "Polygon")} - {len(normalized_guards) if normalized_guards else 0} Guards')
    ax.grid(True, alpha=0.3)
    ax.set_aspect('equal')

    # Output handling
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close(fig)
        return output_path
    else:
        # Convert to base64 for web display
        buf = io.BytesIO()
        plt.savefig(buf, format='png', dpi=150, bbox_inches='tight')
        plt.close(fig)
        buf.seek(0)
        return base64.b64encode(buf.read()).decode('utf-8')


def load_polygon(filename):
    """Load polygon from JSON file"""
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
        return data
    except FileNotFoundError:
        print(f"Error: File {filename} not found")
        return None
    except json.JSONDecodeError:
        print(f"Error: Invalid JSON in {filename}")
        return None


def load_polygon_data(directory='static/polygons'):
    """Load all polygon files from a directory"""
    loaded = []
    if not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)
        return loaded

    for filename in os.listdir(directory):
        if filename.lower().endswith('.json'):
            filepath = os.path.join(directory, filename)
            data = load_polygon(filepath)
            if data:
                # Handle both single polygons and lists
                if isinstance(data, list):
                    loaded.extend(data)
                else:
                    data['filename'] = filename
                    loaded.append(data)
    return loaded


# ============================================================================
# Flask Routes
# ============================================================================

@app.route('/')
def index():
    """Main page"""
    return render_template('index.html')


@app.route('/api/polygons', methods=['GET'])
def get_polygons():
    """Get all loaded polygons"""
    return jsonify(polygons)


@app.route('/api/polygon/<int:index>', methods=['GET'])
def get_polygon(index):
    """Get a specific polygon"""
    if 0 <= index < len(polygons):
        return jsonify(polygons[index])
    return jsonify({'error': 'Polygon not found'}), 404


@app.route('/api/upload', methods=['POST'])
def upload_polygon():
    """Handle file upload"""
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400

    file = request.files['file']
    if file and file.filename.lower().endswith('.json'):
        try:
            data = json.load(file)
            if isinstance(data, list):
                polygons.extend(data)
            else:
                polygons.append(data)
            return jsonify({'success': True, 'count': len(polygons)})
        except json.JSONDecodeError:
            return jsonify({'error': 'Invalid JSON'}), 400

    return jsonify({'error': 'Invalid file type'}), 400


@app.route('/api/visualize/<int:index>', methods=['GET'])
def visualize(index):
    """Visualize a polygon"""
    if 0 <= index < len(polygons):
        guards = request.args.get('guards')
        if guards:
            try:
                guards = json.loads(guards)
            except:
                guards = None

        image_data = visualize_polygon(polygons[index], guards)
        return jsonify({'image': image_data})

    return jsonify({'error': 'Polygon not found'}), 404


@app.route('/api/solve/<int:index>', methods=['POST'])
def solve_polygon(index):
    """
    Solve the art gallery problem for a polygon using the C++ solver.
    """
    if not (0 <= index < len(polygons)):
        return jsonify({'error': 'Polygon not found'}), 404

    polygon = polygons[index]

    # Find the solver executable
    script_dir = os.path.dirname(os.path.abspath(__file__))
    solver_path = os.path.join(script_dir, '..', 'build', 'agp_solver')

    if not os.path.exists(solver_path):
        return jsonify({'error': 'Solver not found. Please build the C++ solver first.'}), 500

    # Create temporary files for input/output
    input_fd, input_path = tempfile.mkstemp(suffix='.json')
    output_path = input_path.replace('.json', '_output.json')

    try:
        # Write polygon to temp file
        with os.fdopen(input_fd, 'w') as f:
            json.dump(polygon, f)

        # Call C++ solver (no timeout - large polygons can take >10 minutes)
        result = subprocess.run(
            [solver_path, input_path, '--output', output_path, '--verbosity', '0'],
            capture_output=True,
            text=True
        )

        # Check for solver errors (return codes: 0=optimal, 2=suboptimal, both are OK)
        if result.returncode not in [0, 2]:
            return jsonify({
                'error': 'Solver failed',
                'returncode': result.returncode,
                'stderr': result.stderr
            }), 500

        # Read solution from output file
        if os.path.exists(output_path):
            with open(output_path, 'r') as f:
                solution = json.load(f)

            guards = solution.get('guards', [])
            return jsonify({
                'guards': guards,
                'count': len(guards),
                'status': solution.get('status', 'optimal' if result.returncode == 0 else 'suboptimal'),
                'iterations': solution.get('iterations', 0),
                'solve_time': solution.get('solve_time_seconds', 0)
            })
        else:
            # No output file - return error
            return jsonify({
                'error': 'No output file generated',
                'stdout': result.stdout,
                'stderr': result.stderr
            }), 500

    except json.JSONDecodeError as e:
        return jsonify({'error': f'Invalid solver output: {e}'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    finally:
        # Cleanup temp files
        if os.path.exists(input_path):
            os.unlink(input_path)
        if os.path.exists(output_path):
            os.unlink(output_path)


@app.route('/api/verify', methods=['POST'])
def verify_solution():
    """
    Verify that a set of guards covers the entire polygon.
    """
    data = request.json
    polygon_data = data.get('polygon')
    guards = data.get('guards', [])

    if not polygon_data:
        return jsonify({'error': 'No polygon provided'}), 400

    vertices = [(v['x'], v['y']) for v in polygon_data['vertices']]
    poly = Polygon(vertices)

    # Simple verification: check if guards are inside polygon
    valid_guards = []
    for g in guards:
        if isinstance(g, dict):
            point = Point(g['x'], g['y'])
        else:
            point = Point(g[0], g[1])

        if poly.contains(point) or poly.boundary.contains(point):
            valid_guards.append(g)

    return jsonify({
        'valid': len(valid_guards) == len(guards),
        'guard_count': len(valid_guards),
        'total_guards': len(guards)
    })


# ============================================================================
# CLI Interface
# ============================================================================

def run_cli():
    """Run command-line interface"""
    parser = argparse.ArgumentParser(description='Art Gallery Problem Visualizer')
    parser.add_argument('input', help='Input polygon JSON file')
    parser.add_argument('--guards', '-g', help='Guards JSON file')
    parser.add_argument('--output', '-o', help='Output image file')
    parser.add_argument('--solve', '-s', action='store_true',
                        help='Solve the problem (requires C++ backend)')
    args = parser.parse_args()

    # Load polygon
    polygon = load_polygon(args.input)
    if not polygon:
        sys.exit(1)

    # Handle list of polygons
    if isinstance(polygon, list):
        polygon = polygon[0]

    # Load guards if provided
    guards = None
    if args.guards:
        with open(args.guards, 'r') as f:
            guards_data = json.load(f)
            guards = [(g['x'], g['y']) for g in guards_data]

    # Determine output path
    output_path = args.output
    if not output_path:
        base_name = os.path.splitext(os.path.basename(args.input))[0]
        output_path = f'{base_name}_visualization.png'

    # Create visualization
    visualize_polygon(polygon, guards, output_path)
    print(f'Visualization saved to {output_path}')


if __name__ == '__main__':
    # Check if running as CLI or web server
    if len(sys.argv) > 1:
        run_cli()
    else:
        # Load default polygons
        polygons = load_polygon_data()

        # Run web server
        print("Starting Art Gallery Problem Visualizer...")
        print("Open http://localhost:5000 in your browser")
        app.run(debug=True, host='0.0.0.0', port=5000)
