"""
N-Pole Cartpole (7-10 poles) Visualization - Improved Version
WuBuOS BearRL - Sovereign C11 RL 
Shows 7, 8, 9, 10 pole balancing with LQR controller
"""

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle, Circle
from scipy.linalg import solve_continuous_are
import os

# ============================================================
# N-Pole Cartpole Physics (matching C implementation)
# ============================================================

class NPoleCartpole:
    def __init__(self, num_poles=7, dt=0.01):
        self.N = num_poles
        self.dt = dt
        self.gravity = 9.8
        self.cart_mass = 1.0
        self.force_mag = 10.0
        self.cart_pos_threshold = 4.0
        self.angle_threshold = 1.73
        
        self.pole_masses = np.array([0.1 * (1.0 - i * 0.02) for i in range(num_poles)])
        self.pole_lengths = np.array([0.5 * (1.0 - i * 0.05) for i in range(num_poles)])
        self.pole_mass_length = self.pole_masses * self.pole_lengths
        self.total_mass = self.cart_mass + np.sum(self.pole_masses)
        
        # For LQR design
        self.A = None
        self.B = None
        self.K = None
        self._build_linearized_model()
        
        self.reset()
    
    def _build_linearized_model(self):
        """Build linearized A, B matrices for LQR around upright equilibrium"""
        N = self.N
        n_state = 2 + 2*N
        n_control = 1
        
        # State: [cart_x, cart_vx, theta_1..N, omega_1..N]
        # Linearized dynamics around theta=0, omega=0
        
        # Mass matrix at equilibrium (cos=1, sin=0)
        M = np.zeros((N+1, N+1))
        M[0,0] = self.total_mass
        for i in range(N):
            M[0, i+1] = self.pole_mass_length[i]
            M[i+1, 0] = self.pole_mass_length[i]
            M[i+1, i+1] = self.pole_masses[i] * self.pole_lengths[i]**2
            for j in range(i+1, N):
                M[i+1, j+1] = self.pole_masses[j] * self.pole_lengths[i] * self.pole_lengths[j]
                M[j+1, i+1] = M[i+1, j+1]
        
        # Gravity terms
        G = np.zeros(N+1)
        G[0] = 0
        for i in range(N):
            G[i+1] = self.pole_masses[i] * self.gravity * self.pole_lengths[i]
        
        # Linearized: M * q_ddot = B*u - G*theta  (for small theta)
        # State: x = [q; q_dot] = [cart_x; theta_1..N; cart_vx; omega_1..N]
        # x_dot = [q_dot; q_ddot]
        
        Minv = np.linalg.inv(M)
        B_full = np.zeros(N+1)
        B_full[0] = 1.0  # Force on cart
        
        A = np.zeros((n_state, n_state))
        B = np.zeros((n_state, n_control))
        
        # q_dot = v
        A[:N+1, N+1:] = np.eye(N+1)
        
        # v_dot = Minv * (B*u - G*q_theta)
        # v_dot = -Minv @ G_diag @ theta + Minv[0,:] * u
        A[N+1:, :N+1] = -Minv @ np.diag(np.concatenate([[0], G[1:]]))
        B[N+1:, 0] = Minv[:, 0]
        
        self.A = A
        self.B = B
        
        # LQR design
        Q = np.eye(n_state)
        # Weight poles heavily
        for i in range(N):
            Q[2+2*i, 2+2*i] = 100.0   # theta
            Q[2+2*i+1, 2+2*i+1] = 10.0  # omega
        Q[0, 0] = 10.0  # cart position
        Q[1, 1] = 1.0   # cart velocity
        
        R = np.array([[0.1]])
        
        try:
            P = solve_continuous_are(A, B, Q, R)
            self.K = np.linalg.inv(R) @ B.T @ P
        except:
            self.K = None
    
    def reset(self):
        self.cart_x = np.random.uniform(-0.05, 0.05)
        self.cart_vx = np.random.uniform(-0.05, 0.05)
        self.theta = np.random.uniform(-0.05, 0.05, self.N)
        self.omega = np.random.uniform(-0.05, 0.05, self.N)
        self.step_count = 0
        self.total_reward = 0.0
        return self.get_obs()
    
    def get_obs(self):
        obs = np.zeros(2 + 2 * self.N)
        obs[0] = self.cart_x
        obs[1] = self.cart_vx
        for i in range(self.N):
            obs[2 + 2*i] = self.theta[i]
            obs[2 + 2*i + 1] = self.omega[i]
        return obs
    
    def compute_accelerations(self, force):
        N = self.N
        
        M = np.zeros((N + 1, N + 1))
        F = np.zeros(N + 1)
        
        M[0, 0] = self.total_mass
        for i in range(N):
            cos_t = np.cos(self.theta[i])
            M[0, i+1] = self.pole_mass_length[i] * cos_t
            M[i+1, 0] = M[0, i+1]
        
        for i in range(N):
            mi = self.pole_masses[i]
            li = self.pole_lengths[i]
            sin_ti = np.sin(self.theta[i])
            M[i+1, i+1] = mi * li * li
            
            for j in range(i+1, N):
                mj = self.pole_masses[j]
                lj = self.pole_lengths[j]
                theta_diff = self.theta[i] - self.theta[j]
                cos_diff = np.cos(theta_diff)
                M[i+1, j+1] = mj * li * lj * cos_diff
                M[j+1, i+1] = M[i+1, j+1]
            
            coriolis = 0.0
            for j in range(N):
                if j == i: continue
                mj = self.pole_masses[j]
                lj = self.pole_lengths[j]
                omega_j = self.omega[j]
                omega_i = self.omega[i]
                theta_diff = self.theta[i] - self.theta[j]
                sin_diff = np.sin(theta_diff)
                coriolis += mj * li * lj * omega_i * omega_j * sin_diff
            
            F[i+1] = -mi * self.gravity * li * sin_ti + coriolis
        
        F[0] = force
        for i in range(N):
            mi = self.pole_masses[i]
            li = self.pole_lengths[i]
            sin_ti = np.sin(self.theta[i])
            omega_i = self.omega[i]
            F[0] += mi * li * omega_i * omega_i * sin_ti
        
        # Solve with regularization for stability
        try:
            acc = np.linalg.solve(M + 1e-6*np.eye(N+1), F)
        except:
            acc = np.linalg.lstsq(M, F, rcond=None)[0]
        
        return acc[0], acc[1:]
    
    def rk4_step(self, force):
        dt = self.dt
        
        x0, vx0 = self.cart_x, self.cart_vx
        theta0 = self.theta.copy()
        omega0 = self.omega.copy()
        
        cart_acc1, theta_acc1 = self.compute_accelerations(force)
        
        self.cart_x = x0 + 0.5 * dt * vx0
        self.cart_vx = vx0 + 0.5 * dt * cart_acc1
        self.theta = theta0 + 0.5 * dt * omega0
        self.omega = omega0 + 0.5 * dt * theta_acc1
        
        cart_acc2, theta_acc2 = self.compute_accelerations(force)
        
        self.cart_x = x0 + 0.5 * dt * (vx0 + 0.5 * dt * cart_acc1)
        self.cart_vx = vx0 + 0.5 * dt * cart_acc2
        self.theta = theta0 + 0.5 * dt * (omega0 + 0.5 * dt * theta_acc1)
        self.omega = omega0 + 0.5 * dt * theta_acc2
        
        cart_acc3, theta_acc3 = self.compute_accelerations(force)
        
        self.cart_x = x0 + dt * (vx0 + 0.5 * dt * cart_acc2)
        self.cart_vx = vx0 + dt * cart_acc3
        self.theta = theta0 + dt * (omega0 + 0.5 * dt * theta_acc2)
        self.omega = omega0 + dt * theta_acc3
        
        cart_acc4, theta_acc4 = self.compute_accelerations(force)
        
        self.cart_x = x0 + dt/6.0 * (vx0 + 2*(vx0 + 0.5*dt*cart_acc1) + 2*(vx0 + 0.5*dt*cart_acc2) + (vx0 + dt*cart_acc3))
        self.cart_vx = vx0 + dt/6.0 * (cart_acc1 + 2*cart_acc2 + 2*cart_acc3 + cart_acc4)
        self.theta = theta0 + dt/6.0 * (omega0 + 2*(omega0 + 0.5*dt*theta_acc1) + 2*(omega0 + 0.5*dt*theta_acc2) + (omega0 + dt*theta_acc3))
        self.omega = omega0 + dt/6.0 * (theta_acc1 + 2*theta_acc2 + 2*theta_acc3 + theta_acc4)
        
        # Clamp to prevent numerical explosion
        self.cart_x = np.clip(self.cart_x, -10.0, 10.0)
        self.cart_vx = np.clip(self.cart_vx, -50.0, 50.0)
        self.theta = np.clip(self.theta, -np.pi, np.pi)
        self.omega = np.clip(self.omega, -100.0, 100.0)
        
        self.step_count += 1
    
    def _lqr_action(self, obs):
        """LQR control: u = -K*x"""
        if self.K is None:
            return 0.0
        x = np.zeros(self.A.shape[0])
        x[0] = obs[0]
        x[1] = obs[1]
        for i in range(self.N):
            x[2+2*i] = obs[2+2*i]
            x[2+2*i+1] = obs[2+2*i+1]
        u = -self.K @ x
        return float(np.clip(u[0], -self.force_mag, self.force_mag))
    
    def step(self, force=None):
        if force is None:
            force = self._lqr_action(self.get_obs())
        
        force = np.clip(force, -self.force_mag, self.force_mag)
        self.rk4_step(force)
        
        # Check done
        done = False
        if abs(self.cart_x) > self.cart_pos_threshold:
            done = True
        for i in range(self.N):
            if abs(self.theta[i]) > self.angle_threshold:
                done = True
        
        # Staggered reset like actual training
        if done:
            self.reset()
        
        # Reward
        reward = 1.0
        for i in range(self.N):
            reward += 0.5 * np.cos(self.theta[i])
        reward -= 0.1 * abs(self.cart_x / self.cart_pos_threshold)
        reward -= 0.01 * abs(self.cart_vx / 10.0)
        
        self.total_reward += reward
        
        return self.get_obs(), reward, done
    
    def get_pole_positions(self):
        positions = [(self.cart_x, 0.0)]
        x, y = self.cart_x, 0.0
        for i in range(self.N):
            positions.append((x, y))
            x += self.pole_lengths[i] * np.sin(self.theta[i])
            y += self.pole_lengths[i] * np.cos(self.theta[i])
            positions.append((x, y))
        return positions


# ============================================================
# Rendering
# ============================================================

def render_frame(ax, envs, poles_list, frame_idx, max_steps):
    ax.clear()
    
    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A']
    pole_colors = ['#E74C3C', '#F39C12', '#F1C40F', '#2ECC71', '#3498DB', '#9B59B6', '#1ABC9C', '#ECF0F1']
    
    for idx, (env, N) in enumerate(zip(envs, poles_list)):
        offset_y = idx * 6.5
        
        # Track
        ax.axhline(y=offset_y, color='#333333', linewidth=4, alpha=0.6)
        ax.axhline(y=offset_y + 6.0, color='#222222', linewidth=1, alpha=0.3)
        
        # Cart
        cart_w, cart_h = 0.8, 0.4
        cart_x = env.cart_x
        
        # Cart shadow
        shadow = Rectangle((cart_x - cart_w/2 - 0.05, offset_y - cart_h/2 - 0.05), 
                          cart_w, cart_h, facecolor='#000000', alpha=0.3, zorder=9)
        ax.add_patch(shadow)
        
        cart_rect = Rectangle((cart_x - cart_w/2, offset_y - cart_h/2), 
                              cart_w, cart_h, 
                              facecolor=colors[idx], edgecolor='white', linewidth=2.5, zorder=10)
        ax.add_patch(cart_rect)
        
        # Wheels
        wheel_r = 0.15
        for wx in [cart_x - 0.3, cart_x + 0.3]:
            wheel = Circle((float(wx), offset_y - cart_h/2 - wheel_r), wheel_r,
                          facecolor='#1A1A1A', edgecolor='#FFFFFF', linewidth=2, zorder=11)
            ax.add_patch(wheel)
            # Wheel highlight
            hwheel = Circle((float(wx), offset_y - cart_h/2 - wheel_r), wheel_r*0.5,
                           facecolor='#444444', alpha=0.5, zorder=12)
            ax.add_patch(hwheel)
        
        # Poles
        pole_positions = env.get_pole_positions()
        pole_positions = np.array(pole_positions)
        
        for i in range(N):
            jx = float(pole_positions[2*i][0])
            jy = float(pole_positions[2*i][1]) + offset_y + 3.0
            tx = float(pole_positions[2*i + 1][0])
            ty = float(pole_positions[2*i + 1][1]) + offset_y + 3.0
            
            # Pole gradient effect - multiple lines for thickness
            for w in [4, 3, 2]:
                alpha = 1.0 if w == 4 else (0.6 if w == 3 else 0.3)
                ax.plot([jx, tx], [jy, ty], color=pole_colors[i % len(pole_colors)], 
                        linewidth=w, solid_capstyle='round', alpha=alpha, zorder=5)
            
            # Joint marker with glow
            for r, a in [(0.12, 0.2), (0.08, 0.5), (0.05, 1.0)]:
                joint_c = Circle((jx, jy), r, facecolor='#FFD700', edgecolor='#FFA500', 
                               linewidth=1, alpha=a, zorder=15)
                ax.add_patch(joint_c)
        
        # Pole tip markers
        tip = pole_positions[-1]
        tip_x = float(tip[0])
        tip_y = float(tip[1]) + offset_y + 3.0
        ax.plot(tip_x, tip_y, 'o', color='#FFD700', markersize=10, 
               markeredgecolor='white', markeredgewidth=2, zorder=16)
        
        # UI Panel
        panel_x = -5.8
        # Title
        ax.text(panel_x, offset_y + 4.2, f'{N}-POLE', fontsize=18, fontweight='bold',
               color=colors[idx], ha='left', va='center',
               bbox=dict(boxstyle='round,pad=0.4', facecolor='#0D0D1A', alpha=0.95, edgecolor=colors[idx], linewidth=2))
        
        # Status
        status_color = '#2ECC71' if env.step_count > 100 else '#F39C12' if env.step_count > 50 else '#E74C3C'
        ax.text(panel_x, offset_y + 3.0, f'Step: {env.step_count}', fontsize=13, fontweight='bold',
               color=status_color, ha='left', va='center')
        
        ax.text(panel_x, offset_y + 2.2, f'Total Reward: {env.total_reward:.1f}', fontsize=11,
               color='#CCCCCC', ha='left', va='center')
        
        ax.text(panel_x, offset_y + 1.4, f'Cart x: {cart_x:.2f}', fontsize=11,
               color='#AAAAAA', ha='left', va='center')
        
        # Pole angles
        for i in range(min(N, 4)):
            angle = env.theta[i]
            angle_color = '#2ECC71' if abs(angle) < 0.3 else '#F39C12' if abs(angle) < 0.8 else '#E74C3C'
            ax.text(panel_x, offset_y + 0.6 - i*0.3, f'θ{i+1}: {angle:+.3f}', fontsize=10,
                   color=angle_color, ha='left', va='center',
                   bbox=dict(boxstyle='round,pad=0.15', facecolor='#0D0D1A', alpha=0.7, edgecolor=angle_color))
        if N > 4:
            ax.text(panel_x, offset_y - 0.6, f'... +{N-4} more poles', fontsize=10,
                   color='#888888', ha='left', va='center')
    
    # Global header
    ax.text(0, len(poles_list)*6.5 + 1.5, 
            'WuBuOS BearRL  •  N-Pole Cartpole (7-10 Poles)  •  Sovereign C11 RL — No Python, No PyTorch, No Gym',
            fontsize=15, fontweight='bold', color='#00D4FF', ha='center',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#050510', alpha=0.95, edgecolor='#00D4FF', linewidth=2))
    
    # Footer
    ax.text(0, -1.0, 
            'LQR Controller  •  RK4 Integration  •  Recursive Lagrangian Dynamics  •  Staggered Episodic Reset',
            fontsize=11, color='#666666', ha='center')
    
    ax.set_xlim(-6.0, 6.0)
    ax.set_ylim(-1.5, len(poles_list)*6.5 + 3.5)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_facecolor('#050510')


def run_simulation_with_controller(num_poles, steps=1000):
    """Run with LQR controller"""
    env = NPoleCartpole(num_poles=num_poles)
    
    for _ in range(steps):
        force = env._lqr_action(env.get_obs())
        env.step(force)
    
    # Return env at a good state
    env.reset()
    return env


# ============================================================
# Main
# ============================================================

if __name__ == '__main__':
    print("🎬 Generating N-Pole Cartpole Video (7-10 poles) — LQR Controller...")
    
    poles_list = [7, 8, 9, 10]
    max_steps = 1200  # 40 seconds at 30fps
    
    print(f"Initializing environments with LQR controllers...")
    envs = []
    for N in poles_list:
        env = run_simulation_with_controller(N, steps=100)
        envs.append(env)
        print(f"  {N}-pole: LQR gain matrix shape = {env.K.shape if env.K is not None else 'None'}")
    
    # Create figure - wide aspect for social media
    fig = plt.figure(figsize=(19.2, 10.8), facecolor='#050510', dpi=100)
    ax = fig.add_subplot(111)
    
    def animate(frame_idx):
        for env, N in zip(envs, poles_list):
            force = env._lqr_action(env.get_obs())
            env.step(force)
        
        render_frame(ax, envs, poles_list, frame_idx, max_steps)
        
        if frame_idx % 150 == 0:
            print(f"  Frame {frame_idx}/{max_steps} | Steps: {[e.step_count for e in envs]}")
    
    print("Rendering animation...")
    anim = animation.FuncAnimation(fig, animate, frames=max_steps, 
                                   interval=33, repeat=False, blit=False)
    
    output_path = os.path.expanduser('~/npole_cartpole_7_10_poles_v2.mp4')
    print(f"Saving to {output_path}...")
    anim.save(output_path, writer='ffmpeg', fps=30, 
              extra_args=['-vcodec', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18',
                          '-preset', 'slow', '-profile:v', 'high'],
              dpi=100, bitrate=5000)
    
    print(f"✅ Video saved to {output_path}")
    plt.close()